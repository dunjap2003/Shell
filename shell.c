#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

int prekinuto = 0;
pid_t pid = 1;

char *obradi_varijablu_path(char *naredba)
{
   char *putanja, *kopijaPutanje;

   putanja = getenv("PATH"); // funkcija getenv traži environment varijable od PATH

   if (putanja)
   {
      kopijaPutanje = strdup(putanja); // pravim kopiju od getenv zato da kasnije sa strtokom ne unistim putanju

      int duljina_naredbe = strlen(naredba); // duljina naredbe nam je potrebna zbog alokacije memorije

      struct stat buffer; // struktura koja ce na kraju provjeriti radi li se o validnom pathu do naredbe

      char *stringiciPutanje = strtok(kopijaPutanje, ":"); // stringovi za koje cemo provjeravati jesu li dio patha do naredbe koju izvrsavamo
      while (stringiciPutanje != NULL)
      {
         int duljinaDirektorija = strlen(stringiciPutanje);
         char *putanjaDoDatoteke = malloc(duljina_naredbe + duljinaDirektorija + 2); // dodajemo 2 za / i terminiranje niza \0

         strcpy(putanjaDoDatoteke, stringiciPutanje); // direktorij od getenv("PATH") za koji sad provjeravamo
         strcat(putanjaDoDatoteke, "/");              // kosa crta za odvajanje naredbe od direktorija
         strcat(putanjaDoDatoteke, naredba);
         strcat(putanjaDoDatoteke, "\0"); // terminiranje niza

         if (stat(putanjaDoDatoteke, &buffer) == 0)
         {
            free(kopijaPutanje);
            return putanjaDoDatoteke;
         }

         else
         {
            free(putanjaDoDatoteke);
            stringiciPutanje = strtok(NULL, ":");
         }
      }

      free(kopijaPutanje);

      return NULL;
   }

   return NULL;
}

void izvediNaredbu(char *argv[])
{
   char *naredbaZaIzvesti = NULL;
   char *pravaNaredba = NULL;

   if (argv)
   {
      naredbaZaIzvesti = argv[0];
      if ((*naredbaZaIzvesti + 0) == '/' || (*naredbaZaIzvesti + 0) == '.') // ako naredba pocinje sa / ili . ne treba se ici u pretragu
      {
         pravaNaredba = naredbaZaIzvesti;
      }

      else
      {
         pravaNaredba = obradi_varijablu_path(naredbaZaIzvesti);
      }

      int status;

      pid = fork();
      if (!pid)
      {
         setpgid(0, 0); // premjestanje djeteta u zasebnu procesnu grupu
         int exe = execve(pravaNaredba, argv, NULL);
         if (exe == -1)
         {
            fprintf(stderr, "fsh: Unknown command: %s\n", naredbaZaIzvesti);
            exit(1);
         }
      }

      else if (pid < 0)
      {
         fprintf(stderr, "fsh: Unable to fork\n");
      }

      else
      {
         do
         {
            pid_t cpid = waitpid(pid, &status, WUNTRACED);
         } while (!WIFEXITED(status) && !WIFSIGNALED(status));
      }
   }
}

void obradaSIGINT(int sig)
{
   prekinuto = 1;

   if (pid > 0)
   {
      kill(pid, SIGINT);
      pid = -1;
   }

   printf("\n");
}

int main(int argc, char *argv[], char *env[])
{
   char *fsh = "fsh> ";
   char *kopijaUnosa = NULL;
   char *unos = NULL;
   size_t vel = 0;
   ssize_t brojProcitanihZnakova;
   char *stringic;
   int brojStringica = 0;
   const char *razbijac = " \n";
   int i;

   (void)argc;
   (void)env;

   struct sigaction act;
   act.sa_handler = obradaSIGINT;
   sigaction(SIGINT, &act, NULL);

   while (1)
   {
      // ispis stringa "fsh> " na početku svake naredbe u ljusci

      prekinuto = 1;
      while (prekinuto)
      { // čitanje onoga što je korisnik unesao
         clearerr(stdin);
         prekinuto = 0;
         printf("%s", fsh);
         brojProcitanihZnakova = getline(&unos, &vel, stdin);
      }

      if (brojProcitanihZnakova == -1 && !prekinuto)
      {
         prekinuto = 0;
         exit(-1);
      }

      kopijaUnosa = malloc(sizeof(char) * brojProcitanihZnakova);
      if (kopijaUnosa == NULL && !prekinuto)
      {
         fprintf(stderr, "fsh: neuspješno alociranje memorije\n");
         return (-1);
      }

      // kopiramo unos u kopijuUnosa jer je strtok destruktivna funkcija koja će "uništiti" string unos
      strcpy(kopijaUnosa, unos);

      stringic = strtok(unos, razbijac);

      while (stringic != NULL)
      {
         brojStringica++;
         stringic = strtok(NULL, razbijac);
      }
      brojStringica++; // broj dijelova naredbe koju je unio korinsnik

      argv = malloc(sizeof(char *) * brojStringica); // alokacija memorije za argv

      stringic = strtok(kopijaUnosa, razbijac); // ponovno rastavljamo unos na male stringove odvojene spaceom kako bismo dobili polje argv
      for (i = 0; stringic != NULL; i++)
      {
         argv[i] = malloc(sizeof(char) * strlen(stringic));
         strcpy(argv[i], stringic);

         stringic = strtok(NULL, razbijac);
      }

      argv[i] = NULL; // kako bismo prilikom izvodenja naredbe znali da se radi o kraju, zanji argument argv mora biti NULL

      if (argv[0] == NULL || strcmp(argv[0], "exit") == 0)
      {
         return 0;
      }

      else if (strcmp(argv[0], "cd") == 0)
      {
         if (argv[1] == NULL)
         {
            continue;
         }

         else if (chdir(argv[1]) != 0)
         {
            fprintf(stderr, "cd: The directory %s does not exist\n", argv[1]);
         }
      }

      else
      {
         izvediNaredbu(argv); // izvodenje neugrađene naredbe
      }
   }

   free(unos);
   free(kopijaUnosa);

   return 0;
}