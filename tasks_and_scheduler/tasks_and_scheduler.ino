/* Inclui bibliotecas */
#include <basic_io_avr.h>
#include <FreeRTOS_AVR.h>
#include <stdlib.h>
#include <stdio.h>

/* Declara a task que envia mensagens para a task gatekeeper. */
static void prvPrintTask( void *pvParameters );

/* Declara a task gatekeeper. */
static void prvStdioGatekeeperTask( void *pvParameters );

/* Define as strings que as tasks e interrupções mostrarão via gatekeeper. */
static char *pcStringsToPrint[] =
{
  "Task 1 ****************************************************\r\n",
  "Task 2 ----------------------------------------------------\r\n",
  "Message printed from the tick hook interrupt ##############\r\n"
};

/*-----------------------------------------------------------*/

/* Declara uma variável do tipo QueueHandle_t. É usada para enviar mensagens das tasks que mostram a informação (print) para o gatekeeper. */
QueueHandle_t xPrintQueue;

void setup( void )
{
  Serial.begin(9600);
  /* Antes da fila ser usada, ela precisa ser criada.
      xQueueCreate (UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
      - uxQueueLength: define o número máximo de itens que a fila pode conter
      - uxItemSize: define o tamanho necessário para manter cada item na fila
      Nesse caso, a fila é criada para conter no máximo 5 ponteiros de caracteres */
  xPrintQueue = xQueueCreate( 5, sizeof( char * ) );

  /* As tasks vão usar um delay aleatório. */
  srand( 567 );

  /* Verifica se a fila foi criada com sucessso. */
  if ( xPrintQueue != NULL )
  {
    /* Cria duas instâncias de tasks que enviam mensagens para o gatekeeper.
        BaseType_t xTaskCreate(TaskFunction_t pvTaskCode, const char * const pcName, unsigned short usStackDepth, void *pvParameters, UBaseType_t uxPriority, TaskHandle_t *pxCreatedTask);
        - pvTaskCode: define o nome da função que implementa a task.
        - pcName: define o nome descritivo para a task.
        - usStackDepth: define o número de palavras a serem alocadas para uso.
        - pvParameters: define o parâmetro que será passado para a task criada.
        - uxPriority: define a prioridade na qual a task criada será executada.
        - pxCreatedTask: é usado para passar um identificador para a task criada. É opcional! */
    xTaskCreate( prvPrintTask, "Print1", 200, ( void * ) 0, 1, NULL );
    xTaskCreate( prvPrintTask, "Print2", 200, ( void * ) 1, 2, NULL );

    /* Cria a task do gatekeeper. */
    xTaskCreate( prvStdioGatekeeperTask, "Gatekeeper", 200, NULL, 0, NULL );

    /* Inicia o scheduler do RTOS (planejador) para que as tasks criadas iniciem sua execução. */
    vTaskStartScheduler();
  }

  /* Se tudo estiver bem, nunca chegaremos aqui, pois o scheduler estará executando as tasks. */
  for ( ;; );
  //  return 0;
}
/*-----------------------------------------------------------*/

static void prvStdioGatekeeperTask( void *pvParameters )
{
  char *pcMessageToPrint;

  /* Esta é a única task que tem permissão para gravar na saída do terminal.
    Qualquer outra task que queira gravar na saída não acessa o terminal diretamente,
    mas envia a saída para essa task. Como apenas uma task tem permissão de escrita,
    não há problemas de exclusão ou serialização mútua a serem considerados dentro dessa task. */
  for ( ;; )
  {
    /* Espera uma mensagem chegar
       xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait);
       - xQueue: define o identificador para a fila da qual o item deve ser recebido.
       - pvBuffer: define o ponteiro para o buffer no qual o item recebido será copiado.
       - xTicksToWait: define a quantidade máxima de tempo que a task deve bloquear aguardando por um item a receber (caso a fila esteja vazia no momento da chamada) */
    xQueueReceive( xPrintQueue, &pcMessageToPrint, portMAX_DELAY );

    /* Não há necessidade de verificar o valor de retorno, pois a task será bloqueada
      indefinidamente e será executada somente quando a mensagem chegar.
      Quando a próxima linha é executada, haverá uma mensagem a ser emitida. */

    //printf( "%s", pcMessageToPrint );
    //fflush( stdout );
    Serial.print(pcMessageToPrint );
    Serial.flush();
    if (Serial.available()) {
      /* Pára o scheduler. Todas as tasks criadas serão automaticamente excluídas e a multitarefa (preventiva ou cooperativa) será interrompida.  */
      vTaskEndScheduler();
    }
    /* Volta a esperar a próxima mensagem. */
  }
}
/*-----------------------------------------------------------*/
extern "C" { // FreeRTOS expectes C linkage
  void vApplicationTickHook( void )
  {
    static int iCount = 0;
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    /* Imprime uma mensagem a cada 200 ticks. A mensagem não será escrita diretamente,
        mas será enviada ao gatekeeper. */
    iCount++;
    if ( iCount >= 200 )
    {
      /* BaseType_t xQueueSendToFrontFromISR(QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t *pxHigherPriorityTaskWoken);
        - xQueue: define o identificador para a fila na qual o item deve ser postado.
        - pvItemToQueue: define um ponteiro para o item que deve ser colocado na fila.
        Nesse caso, o último parâmetro (xHigherPriorityTaskWoken) não é realmente usado, mas ainda deve ser fornecido.
      */
      xQueueSendToFrontFromISR( xPrintQueue, &( pcStringsToPrint[ 2 ] ), (BaseType_t*)&xHigherPriorityTaskWoken );

      /* Reseta o contador. */
      iCount = 0;
    }
  }
}
/*-----------------------------------------------------------*/

static void prvPrintTask( void *pvParameters )
{
  int iIndexToString;

  /* Duas instâncias desta task são criadas para que o índice da string que a task
    enviará para o gatekeeper seja passado no parâmetro da task. */
  iIndexToString = ( int ) pvParameters;

  for ( ;; )
  {
    /* Imprime a string, passando-a para o gatekeeper.
      A fila é criada antes de o scheduler ser iniciado, portanto, já existirá no momento em que essa task for executada.
      BaseType_t xQueueSendToBack(QueueHandle_t xQueue, const void * pvItemToQueue, TickType_t xTicksToWait);
      - xQueue: define o identificador para a fila na qual o item será postado.
      - pvItemToQueue: define um ponteiro para o item que será colocado na fila.
      - xTicksToWait: define a quantidade máxima de tempo que a task deve bloquear aguardando espaço para ficar disponível na fila, caso já esteja cheia. */
    xQueueSendToBack( xPrintQueue, &(pcStringsToPrint[iIndexToString]), 0 );

    /* Espera um tempo aleatório. */
    vTaskDelay( ( rand() & 0x1FF ) );
  }
}
//------------------------------------------------------------------------------
void loop() {}
