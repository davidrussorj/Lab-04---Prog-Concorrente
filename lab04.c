/* Disciplina: Programacao Concorrente
   Prof.: Silvana Rossetto
   Codigo: Comunicação entre threads com variável compartilhada,
           exclusão mútua e sincronização condicional (múltiplos de 1000) */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define INCR_PER_THREAD 100000

long int soma = 0;                // variável compartilhada
pthread_mutex_t mutex;            // lock de exclusão mútua
pthread_cond_t cond_reached;      // sinal: chegamos em múltiplo de 1000
pthread_cond_t cond_printed;      // sinal: múltiplo foi impresso

// Controle de impressão condicionada
int need_print = 0;               // há um valor pendente para imprimir?
long int to_print = 0;            // qual valor precisa ser impresso?

// Finalização coordenada
int done = 0;                     // main sinaliza quando as threads de trabalho acabarem

// --- função executada pelas threads de trabalho
void *ExecutaTarefa (void *arg) {
    long int id = (long int) arg;
    printf("Thread : %ld esta executando...\n", id);

    for (int i = 0; i < INCR_PER_THREAD; i++) {
        pthread_mutex_lock(&mutex);

        // Se há impressão pendente, aguarde até a thread extra imprimir
        while (need_print) {
            pthread_cond_wait(&cond_printed, &mutex);
        }

        // Seção crítica: incrementa soma
        soma++;

        // Ao atingir um múltiplo de 1000, peça impressão e aguarde
        if ((soma % 1000) == 0 && !need_print) {
            need_print = 1;
            to_print = soma;
            // Avisa a thread extra que há um valor a imprimir
            pthread_cond_signal(&cond_reached);

            // Espera até que o valor seja impresso antes de prosseguir
            while (need_print) {
                pthread_cond_wait(&cond_printed, &mutex);
            }
        }

        pthread_mutex_unlock(&mutex);
    }

    printf("Thread : %ld terminou!\n", id);
    pthread_exit(NULL);
}

// --- função executada pela thread de log
void *extra (void *args) {
    printf("Extra : esta executando...\n");

    pthread_mutex_lock(&mutex);
    for (;;) {
        // Aguarda até haver algo para imprimir OU até o término global
        while (!need_print && !done) {
            pthread_cond_wait(&cond_reached, &mutex);
        }

        // Se há um valor pendente, imprime e libera as threads
        if (need_print) {
            long int v = to_print;   // já estamos com o mutex; leitura consistente
            printf("soma = %ld \n", v);
            need_print = 0;
            // Libera todas as threads que estavam aguardando a impressão
            pthread_cond_broadcast(&cond_printed);
            // Continua o laço para procurar novas pendências
            continue;
        }

        // Se chegamos aqui sem pendência e com término sinalizado, sai
        if (done && !need_print) {
            pthread_mutex_unlock(&mutex);
            break;
        }
    }

    printf("Extra : terminou!\n");
    pthread_exit(NULL);
}

// --- fluxo principal
int main(int argc, char *argv[]) {
    pthread_t *tid;          // identificadores das threads
    int nthreads;            // quantidade de threads (linha de comando)

    if (argc < 2) {
        printf("Digite: %s <numero de threads>\n", argv[0]);
        return 1;
    }
    nthreads = atoi(argv[1]);
    if (nthreads <= 0) {
        puts("Numero de threads deve ser positivo.");
        return 1;
    }

    // Aloca vetores (nthreads de trabalho + 1 para a extra)
    tid = (pthread_t*) malloc(sizeof(pthread_t) * (nthreads + 1));
    if (tid == NULL) { puts("ERRO--malloc"); return 2; }

    // Inicializa sincronização
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond_reached, NULL);
    pthread_cond_init(&cond_printed, NULL);

    // Cria threads de trabalho
    for (long int t = 0; t < nthreads; t++) {
        if (pthread_create(&tid[t], NULL, ExecutaTarefa, (void *)t)) {
            printf("--ERRO: pthread_create()\n"); exit(-1);
        }
    }

    // Cria thread de log
    if (pthread_create(&tid[nthreads], NULL, extra, NULL)) {
        printf("--ERRO: pthread_create()\n"); exit(-1);
    }

    // Aguarda as threads de trabalho
    for (int t = 0; t < nthreads; t++) {
        if (pthread_join(tid[t], NULL)) {
            printf("--ERRO: pthread_join() \n"); exit(-1);
        }
    }

    // Sinaliza término global para a thread extra e a acorda
    pthread_mutex_lock(&mutex);
    done = 1;
    pthread_cond_signal(&cond_reached);
    pthread_mutex_unlock(&mutex);

    // Aguarda a extra
    if (pthread_join(tid[nthreads], NULL)) {
        printf("--ERRO: pthread_join() \n"); exit(-1);
    }

    // Libera recursos
    pthread_cond_destroy(&cond_reached);
    pthread_cond_destroy(&cond_printed);
    pthread_mutex_destroy(&mutex);
    free(tid);

    printf("Valor de 'soma' = %ld\n", soma);
    return 0;
}
