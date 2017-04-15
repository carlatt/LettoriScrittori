#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <stdlib.h>
#include <sys/ipc.h>

using namespace std;

//variabili globali
#define MUTEX 0
#define LIBERO 1
#define SCRITTURA 2
#define NUMERO_SEMAFORI 3
#define DIMENSIONE_SHM 2
#define LIMITE 5

int WAIT(int sem_des, int num_semaforo) {
struct sembuf operazioni[1] = {num_semaforo, -1, 0};
return semop(sem_des, operazioni, 1);
}

int SIGNAL(int sem_des, int num_semaforo) {
struct sembuf operazioni[1] = {num_semaforo, +1, 0};
return semop(sem_des, operazioni, 1);
}

//funzione scrittura: scrive un numero casuale minore di 10 all'intarno della memoria condivisa

void scrittura(int shm_id){
    int dato;
    int *p=new int;
    srand(time(NULL));
    p=(int*) shmat(shm_id, 0, SHM_W);
    if (*p==-1){
        cerr<<"errore nella chiamata shmat\n"; 
        exit(0);
    }
    cout<<"\nScrittore: inserisci un carattere: ";
    for(int i=0; i<DIMENSIONE_SHM; i++){
        dato=rand()%10;
        cout<<dato<<" ";
        *p=dato;
        p++;
    }
    cout<<endl<<endl;
}

//funzione lettura: legge ciò che si trova all'interno della memoria condivisa

void lettura(int shm_id){
    int *p=new int;
    p=(int*) shmat(shm_id, 0, SHM_R);
    if (*p==-1){
        cerr<<"errore nella chiamata shmat\n"; 
        exit(0);
    }
    cout<<"Lettore: contenuto della memoria condivisa: ";
    for(int i=0; i<DIMENSIONE_SHM; i++){
        cout<<*p<<" ";
        p++;
    }
    cout<<endl;
}

//funzione scrittore: si mette in coda sul semaforo SCRITTURA
//e tramite la finzione scrittura() scrive nella memoria condivisa
//rilasciando il semaforo

void scrittore(int shm, int sem_id){
    int ret;
    srand(time(NULL));
    ret=WAIT(sem_id, SCRITTURA);
    if (ret==-1){
        cerr<<"errore";
        exit(0);
    }
    sleep(rand()%7);
    cout<<"    WAIT(SCRITTURA)\n";

    scrittura(shm);

    cout<<"    SIGNAL(SCRITTURA)\n";
    ret=SIGNAL(sem_id, SCRITTURA);
    if (ret==-1){
        cerr<<"errore";
        exit(0);
    }
}

//funzione lettore: ottiene la mutua esclusione per le variabili condivise tramite il semaforo MUTEX
//per il conteggio di lettori entrati e usciti, rilascia tutti i semafori se il numero di lettori entrati 
//è uguale al numero dil lettori usciti, tramite il semaforo LIBERO blocca i lettori favorendo 
//l'esecuzione degli scrittori
void lettore(int shm, int sem_id, int  cond){
    int ret, *p=new int, *n=new int, *l=new int;
    srand(time(NULL));
    p=(int*) shmat(cond, 0, SHM_R && SHM_W);
    if (*p==-1){
        cerr<<"errore nella chiamata shmat\n"; 
        exit(0);
    }
    ret=WAIT(sem_id, LIBERO);
    if (ret == -1) {
        cerr << "errore";
        exit(0);
    }
    ret=WAIT(sem_id, MUTEX);
    if (ret == -1) {
        cerr << "errore";
        exit(0);
    } 
    n=p; l=p+1;
    *n=*n+1;
    sleep(rand()%5);
    if (*n==LIMITE){
        ret = SIGNAL(sem_id, MUTEX);
        if (ret == -1) {
            cerr << "errore";
            exit(0);
        } 
    }
    else if (*n!=LIMITE){
        if (*n==1){
            ret = WAIT(sem_id, SCRITTURA);
            if (ret == -1) {
                cerr << "errore";
                exit(0);
                }
            cout<<"    wait(scrittura)\n";
            }
        ret=SIGNAL(sem_id, LIBERO);
        if (ret == -1) {
            cerr << "errore";
            exit(0);
            }
        ret=SIGNAL(sem_id, MUTEX);
        if (ret == -1) {
            cerr << "errore";
            exit(0);
        }
    }

    lettura(shm);

    ret=WAIT(sem_id, MUTEX);
    if (ret == -1) {
        cerr << "errore";
        exit(0);
    }
    *l=*l+1;
    cout<<"       lettori entrati:"<<*n<<", lettori usciti:"<<*l<<endl;
    if(*n==*l && *l==LIMITE){
        *l=0;
        *n=0;
        ret=SIGNAL(sem_id, LIBERO);
        if (ret == -1) {
             cerr << "errore";
             exit(0);
        }

        cout<<"    signal(scrittura)\n";
        ret = SIGNAL(sem_id, SCRITTURA);
        if (ret == -1) {
            cerr << "errore";
            exit(0);
        } 
    }
    else if(*n==*l){
        cout<<"    signal(scrittura)\n";
        ret = SIGNAL(sem_id, SCRITTURA);
        if (ret == -1) {
            cerr << "errore";
            exit(0);
        }
        *l=0;
        *n=0;
    }

    ret=SIGNAL(sem_id, MUTEX);
    if (ret == -1) {
        cerr << "errore";
        exit(0);
    }
}

//corpo del programma principale: permette la creazione di un numero casuale di processi lettori e scrittori
//tramite delle fork() all'interno di un ciclo

int main() {
   srand(time(NULL));

    int n=5+(rand()%20);
    int id_sem_binari, id_shm, var_cond, ret;
    unsigned short sem_values[NUMERO_SEMAFORI];
    key_t sem_key=60, shm_key=50, key_var=40;
    pid_t Processi[n];
    var_cond=shmget(key_var, 2, IPC_CREAT | 0666);
    if (var_cond==-1){
        cerr<<"errore nella chiamata shmget\n";
        return 1;
    }

    id_shm=shmget(shm_key, DIMENSIONE_SHM, IPC_CREAT | 0666);
    if (id_shm==-1){
        cerr<<"errore nella chiamata shmget\n";
        return 1;
    }

    id_sem_binari=semget(sem_key,NUMERO_SEMAFORI,IPC_CREAT | 0666);
    if (id_sem_binari==-1){
        cerr<<"errore nella chiamata semget\n";
        return 1;
    }
    sem_values[MUTEX] = 1;
    sem_values[LIBERO] = 1;
    sem_values[SCRITTURA] = 1;
    ret = semctl(id_sem_binari, 0, SETALL, sem_values);
    if(ret == -1){
        cerr << "Errore nella chiamata semctl" << endl;
        return 1;
    }
    srand(time(NULL));
    for (int i=0;i<n; i++){
        if ((i+10)%5==0){
            Processi[i]=fork();
            if (Processi[i]==0){
                scrittore(id_shm,id_sem_binari);
                exit(0);
            }
        }
        else {           
            Processi[i]=fork();
            if (Processi[i]==0){
                lettore(id_shm,id_sem_binari,  var_cond);
                exit(0);
            }
         }
        sleep(rand()%5);
    }
    for (int i=0;i<n; i++)
        waitpid(Processi[i],NULL,0);

    ret = shmctl(id_shm, IPC_RMID, NULL);
    if(ret == -1){
            cerr << "Errore nella chiamata shmctl" << endl;
            return 1;
    }

    ret = shmctl(var_cond, IPC_RMID, NULL);
    if(ret == -1){
            cerr << "Errore nella chiamata shmctl" << endl;
            return 1;
    }

    ret = semctl(id_sem_binari, NUMERO_SEMAFORI, IPC_RMID, 1);
    if(ret == -1){
            cerr << "Errore nella chiamata semctl" << endl;
            return 1;
    }
    return 0;
}
