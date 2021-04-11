//
// Base on code of Craig Stuart Sapp 
// gcc -O3 -Wall midi_p.c -o midi_p -lpthread 



#include <stdio.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/io.h>


#include <stdlib.h>

#include <pthread.h>         /* for MIDI input interpreter thread */

#define  MIDI_DEVICE  "/dev/midi"


#define base 0x378
#define time 100000
#define LPT_PORT 0x378

// sega mode

FILE *file;

int param_param;

unsigned char OPN2_instrument[38];
unsigned char readbyte;

//Романыч и Такеши

//Формат:

//AL  FB
//AR1 DR1 SR1 RR1 SL1 OL1 KS1 ML1 DT1
//AR2 DR2 SR2 RR2 SL2 OL2 KS2 ML2 DT2
//AR3 DR3 SR3 RR3 SL3 OL3 KS3 ML3 DT3
//AR4 DR4 SR4 RR4 SL4 OL4 KS4 ML4 DT4

// 33 bass
// 78 wow
// 244 caliope
// 248 flute
// 250 pipe organ
// 251 guitars
// 257 synth doom, term


//int freqs[12]= {617,653,692,733,777,823,872,924,979,1037,1099,1164}; 

// ym2608 8MHzmode

int freqs[12]= {654,692,734,777,823,872,924,979,1038,1100,1165,1234};

// global variables:
int                         seqfd;          // sequencer file descriptor
pthread_t                   midiInThread;   // thread blocker for midi input

int noteonset = 0;
int keynumberset = 0;
int velocityset = 0;

int key_number;
int key_velocity;

int aoffset = 0;
int choffset = 0;

int chnumber;
int chcounter;

int chcode ;

int key_ch[6];



int  a_delay, d_delay  = 0;
//int status; 

// function declarations
void* threadFunction(void*);    // thread function for MIDI input interpreter

void slx(int delay){
    
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for(;;) {
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        if((((end.tv_sec*1000000000)+end.tv_nsec) - ((start.tv_sec*1000000000)+start.tv_nsec)) > delay)
        
        break;
        //  printf("Start = %06d, End = %06d\n", (int)start.tv_usec, (int)end.tv_usec );
        // if(start.tv_nsec > end.tv_nsec) 
        //    break;
    }
}
 
 
void ym2612_Reset(void)
{
    outb(0x0, LPT_PORT+2); //0000 0000

    outb(0x1, LPT_PORT+2); //0000 0001

    outb(0x0, LPT_PORT+2); //0000 0000

    outb(0x4, LPT_PORT+2); //0000 0000

    outb(0x5, LPT_PORT+2); //0000 0001

    outb(0x4, LPT_PORT+2); //0000 0000

    usleep(2000);
}



void ym2612_Send(unsigned char addr, unsigned char data, char setA1) //0x52 = A1 LOW, 0x53 = A1 HIGH
{
    // 1000000000/8000000 = 125
    
    a_delay = 0;
    d_delay = 0; //SSG, ADPCM wait cycle  = 0
        
    if ((addr > 0x20 ) && (addr < 0xB7))   { //FM wait cycle > 17
    //ad_delay =  2125;    }
    a_delay =  2200;    }
         
    if (setA1 == 0 ) {
         
        if ((addr > 0x09 ) && (addr < 0x1E)) { //rythm
                a_delay =  2125;    
                //a_delay =  2200;    
        }
    }
         
    if ((addr > 0x20 ) && (addr < 0x9F))   {
    d_delay =  10375;    } //83*125
         
    if ((addr > 0x9F ) && (addr < 0xB7))   {
    d_delay =  5875;    } //47*125
        
         if (setA1 == 0 ) {
             
            if (addr == 0x10 ) { //rythm 576
            d_delay =  72000;    }
            //d_delay =  75000;    }
                
            if ((addr > 0x10 ) && (addr < 0x1E))   { //rythm 83
            d_delay =  10375;    }
            //d_delay =  11250;    }
         }
    
     // printf("%lu",setA1);printf("setA1 \n");
    
  switch(setA1)
  {
      
          
//0 unused 
//0 unused 
//0 bidi off 
//0 irq off 
//x select printer 17 - A0 - hw inverted
//x initialise reset 16 - A1
//x auto linefeed 14 - WR - hw inverted
//x strobe 1 - reset - hw inverted

    
      
    case 0:
    //outb(0x0, LPT_PORT+2); //0000 0000 
    outb(0x9, LPT_PORT+2); //0000 1001
    outb(addr,LPT_PORT);
    outb(0xB, LPT_PORT+2); //0000 1011 
     slx(100);
    outb(0x9, LPT_PORT+2); //0000 1001 
     slx(a_delay);
    outb(0x1, LPT_PORT+2); //0000 0001 
   
    outb(data,LPT_PORT);
    outb(0x3, LPT_PORT+2); //0000 0011 
     slx(100);
    outb(0x1, LPT_PORT+2); //0000 0001 
    slx(d_delay);

    break;

    case 1:
        
    outb(0xD, LPT_PORT+2); //0000 1101 
    outb(addr,LPT_PORT);
    outb(0xF, LPT_PORT+2); //0000 1111
    slx(100);
    outb(0xD, LPT_PORT+2); //0000 1101 
    slx(a_delay);
    outb(0x5, LPT_PORT+2); //0000 0101 
    outb(data,LPT_PORT);
    outb(0x7, LPT_PORT+2); //0000 0111 
    slx(100);
    outb(0x5, LPT_PORT+2); //0000 0101 
     slx(d_delay);

   
    break;
    }
    

}
void init(){
    ym2612_Send(0x22, 0, 0);
    ym2612_Send(0x27, 0, 0);
    ym2612_Send(0x28, 0, 0);
    ym2612_Send(0x28, 1, 0);
    ym2612_Send(0x28, 2, 0);
    ym2612_Send(0x28, 4, 0);
    ym2612_Send(0x28, 5, 0);
    ym2612_Send(0x28, 6, 0);
    
    ym2612_Send(0x2B, 0, 0);
    
    // очень важный регистр
    // переводит в режим OPNA - включает 6 каналов вместо 3 OPN
    
    ym2612_Send(0x29, 0x9F, 0);
}

void playnote(int key_number, int octave, int freq) {
 
   
   //chnumber=3;
    
     printf("\n");
     printf("octave %d\n", octave);
     printf("\n");
     printf("\n");
     printf("freq %d\n", freq);
     printf("\n");    
    
     printf("\n");
     printf("chnumber %d\n", chnumber);
     printf("\n");
     
     key_ch[chnumber]=key_number;
    // aoffset=1;
    // choffset=0;
    // chcode=5;
     
     if (chnumber > 2 ) {chcode=chnumber+1;}
     if (chnumber < 3 ) {chcode=chnumber;}
     
      printf("chcode %d\n", chcode);
    
    if (chnumber >2 ) {choffset=chnumber-3;aoffset=1;}
    if (chnumber <3 ) {choffset=chnumber;aoffset=0;}

    
     printf("\n");
     printf("aoffset %d\n", aoffset);
     printf("\n");
    
    ym2612_Send(0x30+choffset, (OPN2_instrument[10]<<4)+OPN2_instrument[9], aoffset); //Detune (расстройка -3 бита) - Multiply (4 бита) 
    ym2612_Send(0x34+choffset, (OPN2_instrument[19]<<4)+OPN2_instrument[18], aoffset);
    ym2612_Send(0x38+choffset, (OPN2_instrument[28]<<4)+OPN2_instrument[27], aoffset);
    ym2612_Send(0x3C+choffset, (OPN2_instrument[37]<<4)+OPN2_instrument[36], aoffset);
    
    ym2612_Send(0x40+choffset, OPN2_instrument[7], aoffset); // Total level - громкость
    ym2612_Send(0x44+choffset, OPN2_instrument[16], aoffset);
    ym2612_Send(0x48+choffset, OPN2_instrument[25], aoffset);
    ym2612_Send(0x4C+choffset, OPN2_instrument[34], aoffset);

    ym2612_Send(0x50+choffset, (OPN2_instrument[8]<<6)+OPN2_instrument[2], aoffset) ; // Rate (Key) Scaling - Attack Rate
    ym2612_Send(0x54+choffset, (OPN2_instrument[17]<<6)+OPN2_instrument[11], aoffset);
    ym2612_Send(0x58+choffset, (OPN2_instrument[26]<<6)+OPN2_instrument[20], aoffset);
    ym2612_Send(0x5C+choffset, (OPN2_instrument[35]<<6)+OPN2_instrument[29], aoffset);
    
    ym2612_Send(0x60+choffset, OPN2_instrument[3], aoffset); // AM (amon) - D1R (decay rate) 
    ym2612_Send(0x64+choffset, OPN2_instrument[12], aoffset);
    ym2612_Send(0x68+choffset, OPN2_instrument[21], aoffset);
    ym2612_Send(0x6C+choffset, OPN2_instrument[30], aoffset);
    
    ym2612_Send(0x70+choffset, OPN2_instrument[4], aoffset); // (Sustain rate) D2R
    ym2612_Send(0x74+choffset, OPN2_instrument[13], aoffset);
    ym2612_Send(0x78+choffset, OPN2_instrument[22], aoffset);
    ym2612_Send(0x7C+choffset, OPN2_instrument[31], aoffset);
    
    ym2612_Send(0x80+choffset, (OPN2_instrument[6]<<4)+OPN2_instrument[5], aoffset); // D1L (Sustain level)- RR (release rate)
    ym2612_Send(0x84+choffset, (OPN2_instrument[15]<<4)+OPN2_instrument[14], aoffset);
    ym2612_Send(0x88+choffset, (OPN2_instrument[24]<<4)+OPN2_instrument[23], aoffset);
    ym2612_Send(0x8C+choffset, (OPN2_instrument[33]<<4)+OPN2_instrument[32], aoffset);

    ym2612_Send(0x90+choffset, 0x0, aoffset); // SSG EG
    ym2612_Send(0x94+choffset, 0x0, aoffset);
    ym2612_Send(0x98+choffset, 0x0, aoffset);
    ym2612_Send(0x9C+choffset, 0x0, aoffset);
    
    ym2612_Send(0xB0+choffset, (OPN2_instrument[1]<<3)+OPN2_instrument[0], aoffset); //алгоритм + обратная связь
    ym2612_Send(0xB4+choffset, 0xC0, aoffset); // L+R
    
    ym2612_Send(0x28, 0x00+chcode, 0);
    ym2612_Send(0xA4+choffset, (octave <<3)+ ((freq & 0x700)>>8) , aoffset);
    
    ym2612_Send(0xA0+choffset, (freq & 0xFF), aoffset); //частота LSB
    
    ym2612_Send(0x28, 0xF0+chcode, 0);
    
  //  usleep(10000);
   // ym2612_Send(0x28, 0x00+choffset, 0);
    
    chnumber++;
    if (chnumber > 5 ) {chnumber = 0;}
   // if (chnumber > 2 ) {chnumber = 0;}
    
    //ch table  0 1 2 3 4 5
    //          0 1 2 4 5 6
    
}

void offnote(int key_number) {

    int coffset = 0;
    
    for (int i=0;i<6;i++)
    {
     if (key_ch[i] == key_number)   {
     
     printf("off key_number %d\n", key_number);
     printf("key_ch %u\n", key_ch[i]);
    
     if (i > 2) { coffset=i+1;}
     if (i < 3) { coffset = i;}

     printf("ch %u\n", coffset);

     ym2612_Send(0x28, (0x00+coffset), 0);
    }
    }
}
///////////////////////////////////////////////////////////////////////////
   

int main(int argc, char *argv[]) {
   int status;            // for error checking

   // (1) first open the sequencer device for reading.
   //     some examples also have a nonblocking options in 2nd param
   


    file = fopen("OPN2", "rb");

    if(file < 0) {
       printf ("File not found \r");
       exit(1);
    }
    
    if (argv[1]) {param_param = atoi(argv[1]);}
    else {param_param = 0;}
    
    //param_param=1;
    
    printf("Param %d\n", param_param);
    printf("Param %d\n", param_param*38);

    fseek ( file , param_param*38 , 0 );

    
    for (int i=0;i<38;i++){

    fread(&OPN2_instrument[i], sizeof(unsigned char), 1, file);
    
    printf(" OPN data %d", OPN2_instrument[i]);
     
    }
    
    printf("OPN loaded %d\n", chnumber);
     
  
   
    if (ioperm(base,3,1))
    printf("Couldn't get port at %x\n", base), exit(1);
    if (ioperm(base, 3, 1)) {perror("ioperm"); exit(1);}
  

    ym2612_Reset();
    
    init();

    playnote(1,4, freqs[1]);
    sleep(1);
    offnote(1);
       

   seqfd = open(MIDI_DEVICE, O_RDONLY);
   if (seqfd == -1) {
      printf("Error: cannot open %s\n", MIDI_DEVICE);
      exit(1);
   } 

    
   // (2) now start the thread that interpreting incoming MIDI bytes:

   status = pthread_create(&midiInThread, NULL, threadFunction, NULL);
   if (status == -1) {
      printf("Error: unable to create MIDI input thread.\n");
      exit(1);
   } 


   // (3) finally, just wait around for MIDI messages to appear in the
   // input buffer and print them to the screen.

   while (1) {
      // do nothing, just wait around for thread function.
       
       
       
   }
    fclose(file);
    
   return 0;
}


///////////////////////////////////////////////////////////////////////////


//////////////////////////////
//
// threadFunction -- this function is sent to the pthread library
//    functions for running as a separate thread.
//

void* threadFunction(void* x) {
    unsigned char inbyte;         // bytes from sequencer driver
    int status;               // for error checking
    int octave;
    int key_button;
    int command;
    int notep;
   
 
    
    
   
   while (1) {
      status = read(seqfd, &inbyte, sizeof(inbyte));
      if (status < 0) {
      //   printf("Error reading %s\n", MIDI_DEVICE);
      //   exit(1);
      }
 
      if (inbyte != 254) {
          
         printf("received MIDI byte: %d\n", inbyte);
         
         command = inbyte;
         
         if (command == 144) {
          
             noteonset = 1;
             printf("noteon %d\n", inbyte);
             continue;
         }
         
         if ((noteonset == 1 ) && (command < 127) && (keynumberset != 1)) {
         
         printf("key number %d\n", inbyte);
            keynumberset = 1;
            
              key_number = inbyte;
              //key_velocity = inbyte;
            continue;
            }
        if ((noteonset == 1 ) && (command < 127) && (keynumberset == 1 )) {
         
         printf("key velocity %d\n", inbyte);
         
         
         noteonset = 0;
         keynumberset = 0;
         
         key_velocity = inbyte;
         
              if (key_velocity != 0){
                
              velocityset = 1;
           
              key_button=(key_number-24);
              
              octave=0;
              
              if ((key_number) > 36 && (key_number < 49)) {
              octave=1;}
              if ((key_number) > 48 && (key_number < 61)) {
              octave=2;}
              if ((key_number) > 60 && (key_number < 73)) {
              octave=3;}
              if ((key_number) > 72 && (key_number < 85)) {
              octave=4;}
              if ((key_number) > 84 && (key_number < 97)) {
              octave=5;}
              
              printf("button- %d\n", key_button);
               printf("key number- %d\n", key_number);
              
               notep=((key_number-24)-(octave*12));
               
              
              playnote(key_number,octave,freqs[notep-1]);
            }
            if (key_velocity == 0)
            {
                
                printf("noteoff %d\n", inbyte);
                
                noteonset = 0;
                keynumberset = 0;
                velocityset = 0;
                
                offnote(key_number);
            }
              continue;
             
                 
        }
        if (command == 128) {
          
             noteonset = 0;
             printf("noteoff %d\n", inbyte);
             
        }
         
      }
   }
}

