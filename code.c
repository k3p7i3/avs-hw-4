#include "pthread.h"
#include "semaphore.h"

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"

// с помощью define заводим "константу" - количество потоков-философов и вилок-семафоров
#define PHIL_NUMBER 5

int stop_flag = 0; // когда stop_flag равен true, значит, пора закругляться и завершать работу программы

sem_t forks[PHIL_NUMBER]; // семафоры, каждый соответствует одной вилке

/*  семафор, который помогает избежать фатальных ситуаций-
    запрещает всем потокам одновременно брать вилки  */
sem_t take_forks;

pthread_rwlock_t rwlock; // rwlock защищает ввод-вывод от захваат сразу несколькими потоками

// файл, в который нужно выводить данные (может быть null)
FILE *file_output = NULL;

// входные данные - ограничения затрат времени на eat и meditate
int min_meditate_time;
int max_meditate_time;
int min_eat_time;
int max_eat_time;

//  случайно генерируем время (в секундах) в открезке [min_time; max_time]
int generate_time(int min_time, int max_time) {
    return rand() % (max_time - min_time + 1) + min_time;
}

/* функция, имплементирующая "размышления" философа - 
    поток просто засыпает на определенное время */
void meditate(int philosopher_id) {
    // генерируем время - сколько поток будет размышлять
    int meditate_time = generate_time(min_meditate_time, max_meditate_time);

    // выводим сообщение в поток, защищая ресурс rwlock
    pthread_rwlock_rdlock(&rwlock) ;
    if (file_output) {
        fprintf(file_output, "Philosopher %d started meditating for %d sec...\n", philosopher_id + 1, meditate_time);
    }
    printf("Philosopher %d started meditating for %d sec...\n", philosopher_id + 1, meditate_time);

    pthread_rwlock_unlock(&rwlock) ;

    sleep(meditate_time); // философ-поток размышляет в течение meditate_time секунд

    // выводим сообщение в поток, защищая ресурс rwlock
    pthread_rwlock_rdlock(&rwlock) ;
    if (file_output) {
        fprintf(file_output, "Philosopher %d finished meditating\n", philosopher_id + 1);
    }
    printf("Philosopher %d finished meditating\n", philosopher_id + 1);

    pthread_rwlock_unlock(&rwlock) ;
}


/*  функция, имплементирующая "поедание спагетти" философом -
    поток-философ ждет, пока не сможет завладеть двумя вилками по соседству,
    только потом начинает есть, а после освобождает вилки   */
void eat(int philosopher_id) {
    int first_fork_id = philosopher_id;
    int second_fork_id = (philosopher_id + 1) % PHIL_NUMBER;

    /*  не разрешаем всем философам одновременно брать вилки
        (иначе может случиться фатальная ситуация, когда у каждого философа по 1 вилке),
        поэтому "упаковываем" взятие двух вилок в одну неразрывную операцию
        (если 4 философа одновременно выбирают вилки, найдется хотя бы один философ, который может взять обе вилки и покушать),
        а с 5 философами такое уже не гарантируется
    */
    sem_wait(&take_forks);

    sem_wait(forks + first_fork_id);    // поток забирает ресурс - первую вилку (или ждет, пока вилка не освободится..)
    sem_wait(forks + second_fork_id);   // аналагично поток забирает вторую вилку

    sem_post(&take_forks);

    //  генерируем время - сколько времени займет "трапеза" у данного потока
    int eating_time = generate_time(min_eat_time, max_eat_time);

    // выводим сообщение в поток, защищая ресурс rwlock
    pthread_rwlock_rdlock(&rwlock) ;
    if (file_output) {
        fprintf(file_output, "Philosopher %d started eating spagetti for %d sec...\n", philosopher_id + 1, eating_time);
    }
    printf("Philosopher %d started eating spagetti for %d sec...\n", philosopher_id + 1, eating_time);
    
    pthread_rwlock_unlock(&rwlock) ;

    sleep(eating_time);    // философ-поток ест спагетти в течение eating_time секунд

    // выводим сообщение в поток, защищая ресурс rwlock
    pthread_rwlock_rdlock(&rwlock) ;
    if (file_output) {
        fprintf(file_output, "Philosopher %d finished eating spagetti\n", philosopher_id + 1);
    }
    printf("Philosopher %d finished eating spagetti\n", philosopher_id + 1);
    
    pthread_rwlock_unlock(&rwlock) ;

    // освобождаем вилки-семафоры
    sem_post(forks + first_fork_id);
    sem_post(forks + second_fork_id);
}


void *philosopher(void *args) {
    // получаем порядковый номер философа (от 0 до 4)
    int philosopher_id = *((int *)args);

    // "жизненный цикл" философа - чередование еды и философствования
    // пока пользователь не скажет завершать программу
    while (!stop_flag) {
        eat(philosopher_id);
        meditate(philosopher_id);
    }
    return NULL;
}

// проверяем входные аргументы на корректность
void check_args() {
    if (min_meditate_time <= 0 || max_meditate_time < min_meditate_time || 
        min_eat_time <= 0 || max_eat_time < min_eat_time) {

        fprintf(stderr, "Arguments aren't correct!");
        exit(-1);
    }
}

// в этой функции разбираемся, в каком формате будут поступать входные данные
void handle_input_format(int argc, char **argv) {
        // обрабатываем входные данные и способ ввода-вывода
    if (argc < 2) {
        // нужно задать хотя бы способ ввода
        fprintf(stderr, "At least 1 argument excpected!");
        exit(-1);
    }

    // если была задана опция --console_input, тогда вводим данные с консоли
    if (!strcmp(argv[1], "--console_input")) {
        scanf("%d%d%d%d", &min_eat_time, &max_eat_time, &min_meditate_time, &max_meditate_time);
        return;
    }

    // если была задана опция --command_line_input, то тогда читаем данные из командной строки (из argv)
    if (!strcmp(argv[1], "--command_input")) {
        if (argc < 6) {
            fprintf(stderr, "After --command_line_input there must be 4 arguments!");
            exit(-1);
        }
        // читаем входные данные из командной строки
        min_eat_time = atoi(argv[2]);
        max_eat_time = atoi(argv[3]);
        min_meditate_time = atoi(argv[4]);
        max_meditate_time = atoi(argv[5]);
        return;
    }

    if (!strcmp(argv[1], "--file_input")) {
        // считываем имя файла для чтения входных данных и открываем его
        if (argc < 3) {
            fprintf(stderr, "After --file_input there must be file name!");
            exit(-1);
        }
        FILE *file_input = fopen(argv[2], "r");
        if (!file_input) {
            fprintf(stderr, "Couldn't open input file!");
            exit(-1);
        }

        // читаем входные данные из файла и закрываем файл
        fscanf(file_input, "%d%d%d%d", &min_eat_time, &max_eat_time, &min_meditate_time, &max_meditate_time);
        fclose(file_input);
        return;
    }

    fprintf(stderr, "You haven't specify input format!");
    exit(-1);
}

// в этой функции разбираемся, в каком формате будем выводить данные
void handle_output_format(int argc, char **argv) {
    // ищем опцию --file_output - вывод в файл (иначе будем выводить в консоль)
    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--file_output"))
        {
            if (argc <= i + 1) {
                fprintf(stderr, "After --file_output there must be file name!");
                exit(-1);
            }

            file_output = fopen(argv[i + 1], "w");
            if (!file_output) {
                fprintf(stderr, "Couldn't open output file!");
                exit(-1);
            }
        }
    }
}

int main(int argc, char **argv) {

    // разбираемся с аргументами командной строки
    handle_input_format(argc, argv);
    handle_output_format(argc, argv);

    pthread_t philosophers[PHIL_NUMBER];    // pthread потоки - философы

    // инициалируем использующиеся семафоры
    for (int i = 0; i < PHIL_NUMBER; ++i) {
        sem_init(&forks[i], 0, 1);
    }
    sem_init(&take_forks, 0, 4);
    pthread_rwlock_init(&rwlock, NULL) ;

    // создаем 5 потоков - философов и начинаем их жизненный "цикл", передавая в функцию id потока
    int num[PHIL_NUMBER];
    for (int i = 0; i < PHIL_NUMBER; ++i) {
        num[i] = i;
        pthread_create(&philosophers[i], NULL, philosopher, (void *)(num + i));
    }

    // как только прочитали из терминала 'q' - пора выходить
    char quit_signal;
    while (scanf("%c", &quit_signal)) {
        if (quit_signal == 'q') {
            stop_flag = 1;
            break;
        }
    }

    // синхронизируем потоки
    for (int i = 0; i < PHIL_NUMBER; ++i) {
        pthread_join(philosophers[i], NULL);
    }

    // уничтожаем используемые семаформы
    for (int i = 0; i < PHIL_NUMBER; ++i) {
        sem_destroy(&forks[i]);
    }
    sem_destroy(&take_forks);

    if (file_output) {
        fclose(file_output);
    }
    return 0;
}