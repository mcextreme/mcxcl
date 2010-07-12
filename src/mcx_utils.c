/*******************************************************************************
**
**  Monte Carlo eXtreme (MCX)  - GPU accelerated Monte Carlo 3D photon migration
**      -- OpenCL edition
**  Author: Qianqian Fang <fangq at nmr.mgh.harvard.edu>
**
**  Reference (Fang2009):
**        Qianqian Fang and David A. Boas, "Monte Carlo Simulation of Photon 
**        Migration in 3D Turbid Media Accelerated by Graphics Processing 
**        Units," Optics Express, vol. 17, issue 22, pp. 20178-20190 (2009)
**
**  mcx_utils.c: configuration and command line option processing unit
**
**  Unpublished work, see LICENSE.txt for details
**
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mcx_utils.h"

char shortopt[]={'h','i','f','n','m','t','T','s','a','g','b','B','D','G','W','z',
                 'd','r','S','p','e','U','R','l','L','I','o','c','k','v','\0'};
const char *fullopt[]={"--help","--interactive","--input","--photon","--move",
                 "--thread","--blocksize","--session","--array","--gategroup",
                 "--reflect","--reflect3","--device","--devicelist","--workload","--srcfrom0",
		 "--savedet","--repeat","--save2pt","--printlen","--minenergy",
                 "--normalize","--skipradius","--log","--listgpu",
                 "--printgpu","--root","--cpu","--kernel","--verbose",""};


void mcx_initcfg(Config *cfg){
     cfg->medianum=0;
     cfg->detnum=0;
     cfg->dim.x=0;
     cfg->dim.y=0;
     cfg->dim.z=0;
     cfg->nblocksize=128;
     cfg->nphoton=0;
     cfg->nthread=0;
     cfg->seed=0;
     cfg->isrowmajor=0; /* default is Matlab array*/
     cfg->maxgate=1;
     cfg->isreflect=1;
     cfg->isref3=0;
     cfg->isnormalized=1;
     cfg->issavedet=1;
     cfg->respin=1;
     cfg->issave2pt=1;
     cfg->isgpuinfo=0;

     cfg->prop=NULL;
     cfg->detpos=NULL;
     cfg->vol=NULL;
     cfg->session[0]='\0';
     cfg->printnum=0;
     cfg->minenergy=0.f;
     cfg->flog=stdout;
     cfg->sradius=0.f;
     cfg->rootpath[0]='\0';
     cfg->iscpu=0;
     cfg->isverbose=0;
     cfg->clsource='\0';
     memset(cfg->deviceid,0,MAX_DEVICE);
     memset(cfg->workload,0,MAX_DEVICE*sizeof(float));
     cfg->deviceid[0]='a'; /*use the first GPU device by default*/
     strcpy(cfg->kernelfile,"mcx_core.cl");
     cfg->issrcfrom0=0;
}

void mcx_clearcfg(Config *cfg){
     if(cfg->medianum)
     	free(cfg->prop);
     if(cfg->detnum)
     	free(cfg->detpos);
     if(cfg->dim.x && cfg->dim.y && cfg->dim.z)
        free(cfg->vol);
     if(cfg->clsource)
        free(cfg->clsource);

     mcx_initcfg(cfg);
}

void mcx_savedata(float *dat, int len, int doappend, Config *cfg){
     FILE *fp;
     char name[MAX_PATH_LENGTH];
     sprintf(name,"%s.mc2",cfg->session);
     if(doappend){
        fp=fopen(name,"ab");
     }else{
        fp=fopen(name,"wb");
     }
     if(fp==NULL){
	mcx_error(-2,"can not save data to disk");
     }
     fwrite(dat,sizeof(float),len,fp);
     fclose(fp);
}

void mcx_printlog(Config *cfg, const char *str){
     if(cfg->flog>0){ /*stdout is 1*/
         fprintf(cfg->flog,"%s\n",str);
     }
}

void mcx_normalize(float field[], float scale, int fieldlen){
     int i;
     for(i=0;i<fieldlen;i++){
         field[i]*=scale;
     }
}

void mcx_error(int id,const char *msg){
     fprintf(stdout,"MCX ERROR(%d):%s\n",id,msg);
     exit(id);
}

void mcx_createfluence(float **fluence, Config *cfg){
     mcx_clearfluence(fluence);
     *fluence=(float*)calloc(cfg->dim.x*cfg->dim.y*cfg->dim.z,cfg->maxgate*sizeof(float));
}

void mcx_clearfluence(float **fluence){
     if(*fluence) free(*fluence);
}

void mcx_readconfig(const char *fname, Config *cfg){
     if(fname[0]==0){
     	mcx_loadconfig(stdin,cfg);
        if(cfg->session[0]=='\0'){
		strcpy(cfg->session,"default");
	}
     }
     else{
     	FILE *fp=fopen(fname,"rt");
	if(fp==NULL) mcx_error(-2,"can not load the specified config file");
	mcx_loadconfig(fp,cfg); 
	fclose(fp);
        if(cfg->session[0]=='\0'){
		strcpy(cfg->session,fname);
	}
     }
}

void mcx_writeconfig(const char *fname, Config *cfg){
     if(fname[0]==0)
     	mcx_saveconfig(stdout,cfg);
     else{
     	FILE *fp=fopen(fname,"wt");
	if(fp==NULL) mcx_error(-2,"can not write to the specified config file");
	mcx_saveconfig(fp,cfg);     
	fclose(fp);
     }
}

void mcx_loadconfig(FILE *in, Config *cfg){
     int i,gates,idx1d;
     char filename[MAX_PATH_LENGTH]={0}, comment[MAX_PATH_LENGTH];
     
     if(in==stdin)
     	fprintf(stdout,"Please specify the total number of photons: [1000000]\n\t");
     fscanf(in,"%d", &(i) ); 
     if(cfg->nphoton==0) cfg->nphoton=i;
     fgets(comment,MAX_PATH_LENGTH,in);
     if(in==stdin)
     	fprintf(stdout,"%d\nPlease specify the random number generator seed: [1234567]\n\t",cfg->nphoton);
     fscanf(in,"%d", &(cfg->seed) );
     fgets(comment,MAX_PATH_LENGTH,in);
     if(in==stdin)
     	fprintf(stdout,"%d\nPlease specify the position of the source: [10 10 5]\n\t",cfg->seed);
     fscanf(in,"%f %f %f", &(cfg->srcpos.x),&(cfg->srcpos.y),&(cfg->srcpos.z) );
     fgets(comment,MAX_PATH_LENGTH,in);
     if(in==stdin)
     	fprintf(stdout,"%f %f %f\nPlease specify the normal direction of the source fiber: [0 0 1]\n\t",
                                   cfg->srcpos.x,cfg->srcpos.y,cfg->srcpos.z);
     if(!cfg->issrcfrom0){
     	cfg->srcpos.x--;cfg->srcpos.y--;cfg->srcpos.z--; /*convert to C index, grid center*/
     }
     fscanf(in,"%f %f %f", &(cfg->srcdir.x),&(cfg->srcdir.y),&(cfg->srcdir.z) );
     fgets(comment,MAX_PATH_LENGTH,in);
     if(in==stdin)
     	fprintf(stdout,"%f %f %f\nPlease specify the time gates in seconds (start end and step) [0.0 1e-9 1e-10]\n\t",
                                   cfg->srcdir.x,cfg->srcdir.y,cfg->srcdir.z);
     fscanf(in,"%f %f %f", &(cfg->tstart),&(cfg->tend),&(cfg->tstep) );
     fgets(comment,MAX_PATH_LENGTH,in);

     if(in==stdin)
     	fprintf(stdout,"%f %f %f\nPlease specify the path to the volume binary file:\n\t",
                                   cfg->tstart,cfg->tend,cfg->tstep);
     if(cfg->tstart>cfg->tend || cfg->tstep==0.f){
         mcx_error(-9,"incorrect time gate settings");
     }
     gates=(int)((cfg->tend-cfg->tstart)/cfg->tstep+0.5);
     if(cfg->maxgate>gates)
	 cfg->maxgate=gates;

     fscanf(in,"%s", filename);
     if(cfg->rootpath[0]){
#ifdef WIN32
         sprintf(comment,"%s\\%s",cfg->rootpath,filename);
#else
         sprintf(comment,"%s/%s",cfg->rootpath,filename);
#endif
         strncpy(filename,comment,MAX_PATH_LENGTH);
     }
     fgets(comment,MAX_PATH_LENGTH,in);

     if(in==stdin)
     	fprintf(stdout,"%s\nPlease specify the x voxel size (in mm), x dimension, min and max x-index [1.0 100 1 100]:\n\t",filename);
     fscanf(in,"%f %d %d %d", &(cfg->steps.x),&(cfg->dim.x),&(cfg->crop0.x),&(cfg->crop1.x));
     fgets(comment,MAX_PATH_LENGTH,in);

     if(in==stdin)
     	fprintf(stdout,"%f %d %d %d\nPlease specify the y voxel size (in mm), y dimension, min and max y-index [1.0 100 1 100]:\n\t",
                                  cfg->steps.x,cfg->dim.x,cfg->crop0.x,cfg->crop1.x);
     fscanf(in,"%f %d %d %d", &(cfg->steps.y),&(cfg->dim.y),&(cfg->crop0.y),&(cfg->crop1.y));
     fgets(comment,MAX_PATH_LENGTH,in);

     if(in==stdin)
     	fprintf(stdout,"%f %d %d %d\nPlease specify the z voxel size (in mm), z dimension, min and max z-index [1.0 100 1 100]:\n\t",
                                  cfg->steps.y,cfg->dim.y,cfg->crop0.y,cfg->crop1.y);
     fscanf(in,"%f %d %d %d", &(cfg->steps.z),&(cfg->dim.z),&(cfg->crop0.z),&(cfg->crop1.z));
     fgets(comment,MAX_PATH_LENGTH,in);

     if(in==stdin)
     	fprintf(stdout,"%f %d %d %d\nPlease specify the total types of media:\n\t",
                                  cfg->steps.z,cfg->dim.z,cfg->crop0.z,cfg->crop1.z);
     fscanf(in,"%d", &(cfg->medianum));
     cfg->medianum++;
     fgets(comment,MAX_PATH_LENGTH,in);

     if(in==stdin)
     	fprintf(stdout,"%d\n",cfg->medianum);
     cfg->prop=(Medium*)malloc(sizeof(Medium)*cfg->medianum);
     cfg->prop[0].mua=0.f; /*property 0 is already air*/
     cfg->prop[0].mus=0.f;
     cfg->prop[0].g=0.f;
     cfg->prop[0].n=1.f;
     for(i=1;i<cfg->medianum;i++){
        if(in==stdin)
		fprintf(stdout,"Please define medium #%d: mus(1/mm), anisotropy, mua(1/mm) and refractive index: [1.01 0.01 0.04 1.37]\n\t",i);
     	fscanf(in, "%f %f %f %f", &(cfg->prop[i].mus),&(cfg->prop[i].g),&(cfg->prop[i].mua),&(cfg->prop[i].n));
        fgets(comment,MAX_PATH_LENGTH,in);
        if(in==stdin)
		fprintf(stdout,"%f %f %f %f\n",cfg->prop[i].mus,cfg->prop[i].g,cfg->prop[i].mua,cfg->prop[i].n);
     }
     if(in==stdin)
     	fprintf(stdout,"Please specify the total number of detectors and fiber diameter (in mm):\n\t");
     fscanf(in,"%d %f", &(cfg->detnum), &(cfg->detradius));
     fgets(comment,MAX_PATH_LENGTH,in);
     if(in==stdin)
     	fprintf(stdout,"%d %f\n",cfg->detnum,cfg->detradius);
     cfg->detpos=(float4*)malloc(sizeof(float4)*cfg->detnum);
     cfg->issavedet=(cfg->detpos>0);
     for(i=0;i<cfg->detnum;i++){
        if(in==stdin)
		fprintf(stdout,"Please define detector #%d: x,y,z (in mm): [5 5 5 1]\n\t",i);
     	fscanf(in, "%f %f %f", &(cfg->detpos[i].x),&(cfg->detpos[i].y),&(cfg->detpos[i].z));
        if(!cfg->issrcfrom0){
	   cfg->detpos[i].x--;cfg->detpos[i].y--;cfg->detpos[i].z--;  /*convert to C index*/
	}
        fgets(comment,MAX_PATH_LENGTH,in);
        if(in==stdin)
		fprintf(stdout,"%f %f %f\n",cfg->detpos[i].x,cfg->detpos[i].y,cfg->detpos[i].z);
     }
     if(filename[0]){
        mcx_loadvolume(filename,cfg);
        if(cfg->srcpos.x<0.f || cfg->srcpos.y<0.f || cfg->srcpos.z<0.f ||
            cfg->srcpos.x>=cfg->dim.x || cfg->srcpos.y>=cfg->dim.y || cfg->srcpos.z>=cfg->dim.z)
                mcx_error(-4,"source position is outside of the volume");

	idx1d=cfg->isrowmajor?((int)floor(cfg->srcpos.x)*cfg->dim.y*cfg->dim.z+(int)floor(cfg->srcpos.y)*cfg->dim.z+(int)floor(cfg->srcpos.z)):\
                      ((int)floor(cfg->srcpos.z)*cfg->dim.y*cfg->dim.x+(int)floor(cfg->srcpos.y)*cfg->dim.x+(int)floor(cfg->srcpos.x));
	
        /* if the specified source position is outside the domain, move the source
	   along the initial vector until it hit the domain */
	if(cfg->vol && cfg->vol[idx1d]==0){
                printf("source (%f %f %f) is located outside the domain, vol[%d]=%d\n",
		      cfg->srcpos.x,cfg->srcpos.y,cfg->srcpos.z,idx1d,cfg->vol[idx1d]);
		while(cfg->vol[idx1d]==0){
			cfg->srcpos.x+=cfg->srcdir.x;
			cfg->srcpos.y+=cfg->srcdir.y;
			cfg->srcpos.z+=cfg->srcdir.z;
			printf("fixing source position to (%f %f %f)\n",cfg->srcpos.x,cfg->srcpos.y,cfg->srcpos.z);
			idx1d=cfg->isrowmajor?(int)(floor(cfg->srcpos.x)*cfg->dim.y*cfg->dim.z+floor(cfg->srcpos.y)*cfg->dim.z+floor(cfg->srcpos.z)):\
                		      (int)(floor(cfg->srcpos.z)*cfg->dim.y*cfg->dim.x+floor(cfg->srcpos.y)*cfg->dim.x+floor(cfg->srcpos.x));
		}
	}
     }else{
     	mcx_error(-4,"one must specify a binary volume file in order to run the simulation");
     }
}

void mcx_saveconfig(FILE *out, Config *cfg){
     int i;

     fprintf(out,"%d\n", (cfg->nphoton) ); 
     fprintf(out,"%d\n", (cfg->seed) );
     fprintf(out,"%f %f %f\n", (cfg->srcpos.x),(cfg->srcpos.y),(cfg->srcpos.z) );
     fprintf(out,"%f %f %f\n", (cfg->srcdir.x),(cfg->srcdir.y),(cfg->srcdir.z) );
     fprintf(out,"%f %f %f\n", (cfg->tstart),(cfg->tend),(cfg->tstep) );
     fprintf(out,"%f %d %d %d\n", (cfg->steps.x),(cfg->dim.x),(cfg->crop0.x),(cfg->crop1.x));
     fprintf(out,"%f %d %d %d\n", (cfg->steps.y),(cfg->dim.y),(cfg->crop0.y),(cfg->crop1.y));
     fprintf(out,"%f %d %d %d\n", (cfg->steps.z),(cfg->dim.z),(cfg->crop0.z),(cfg->crop1.z));
     fprintf(out,"%d", (cfg->medianum));
     for(i=0;i<cfg->medianum;i++){
     	fprintf(out, "%f %f %f %f\n", (cfg->prop[i].mus),(cfg->prop[i].g),(cfg->prop[i].mua),(cfg->prop[i].n));
     }
     fprintf(out,"%d", (cfg->detnum));
     for(i=0;i<cfg->detnum;i++){
     	fprintf(out, "%f %f %f %f\n", (cfg->detpos[i].x),(cfg->detpos[i].y),(cfg->detpos[i].z),(cfg->detpos[i].w));
     }
}

void mcx_loadvolume(char *filename,Config *cfg){
     int datalen,res;
     FILE *fp=fopen(filename,"rb");
     if(fp==NULL){
     	     mcx_error(-5,"the specified binary volume file does not exist");
     }
     if(cfg->vol){
     	     free(cfg->vol);
     	     cfg->vol=NULL;
     }
     datalen=cfg->dim.x*cfg->dim.y*cfg->dim.z;
     cfg->vol=(unsigned char*)malloc(sizeof(unsigned char)*datalen);
     res=fread(cfg->vol,sizeof(unsigned char),datalen,fp);
     fclose(fp);
     if(res!=datalen){
     	 mcx_error(-6,"file size does not match specified dimensions");
     }
}

int mcx_readarg(int argc, char *argv[], int id, void *output,const char *type){
     /*
         when a binary option is given without a following number (0~1), 
         we assume it is 1
     */
     if(strcmp(type,"char")==0 && (id>=argc-1||(argv[id+1][0]<'0'||argv[id+1][0]>'9'))){
	*((char*)output)=1;
	return id;
     }
     if(id<argc-1){
         if(strcmp(type,"char")==0)
             *((char*)output)=atoi(argv[id+1]);
	 else if(strcmp(type,"int")==0)
             *((int*)output)=atoi(argv[id+1]);
	 else if(strcmp(type,"float")==0)
             *((float*)output)=atof(argv[id+1]);
	 else if(strcmp(type,"string")==0)
	     strcpy((char *)output,argv[id+1]);
	 else if(strcmp(type,"bytenumlist")==0){
	     char *nexttok,*numlist=(char *)output;
	     int len=0,i;
	     nexttok=strtok(argv[id+1]," ,;");
	     while(nexttok){
    		 numlist[len++]=(char)(atoi(nexttok)); /*device id<256*/
		 for(i=0;i<len-1;i++) /* remove duplicaetd ids */
		    if(numlist[i]==numlist[len-1]){
		       numlist[--len]='\0';
		       break;
		    }
		 nexttok=strtok(NULL," ,;");
		 /*if(len>=MAX_DEVICE) break;*/
	     }
	 }else if(strcmp(type,"floatlist")==0){
	     char *nexttok;
	     float *numlist=(float *)output;
	     int len=0;
	     nexttok=strtok(argv[id+1]," ,;");
	     while(nexttok){
    		 numlist[len++]=atof(nexttok); /*device id<256*/
		 nexttok=strtok(NULL," ,;");
	     }
	 }
     }else{
     	 mcx_error(-1,"incomplete input");
     }
     return id+1;
}
int mcx_remap(char *opt){
    int i=0;
    while(shortopt[i]!='\0'){
	if(strcmp(opt,fullopt[i])==0){
		opt[1]=shortopt[i];
		opt[2]='\0';
		return 0;
	}
	i++;
    }
    return 1;
}
void mcx_parsecmd(int argc, char* argv[], Config *cfg){
     int i=1,isinteractive=1,issavelog=0;
     char filename[MAX_PATH_LENGTH]={0};
     char logfile[MAX_PATH_LENGTH]={0};

     if(argc<=1){
     	mcx_usage(argv[0]);
     	exit(0);
     }
     while(i<argc){
     	    if(argv[i][0]=='-'){
		if(argv[i][1]=='-'){
			if(mcx_remap(argv[i])){
				mcx_error(-2,"unknown verbose option");
			}
		}
	        switch(argv[i][1]){
		     case 'h': 
		                mcx_usage(argv[0]);
				exit(0);
		     case 'i':
				if(filename[0]){
					mcx_error(-2,"you can not specify both interactive mode and config file");
				}
		     		isinteractive=1;
				break;
		     case 'f': 
		     		isinteractive=0;
		     	        i=mcx_readarg(argc,argv,i,filename,"string");
				break;
                     /* 
		        ideally we may support -n option to specify photon numbers,
		        however, we found using while-loop for -n will cause some 
			troubles for MT RNG, and timed-out error for some other cases.
			right now, -n is only an alias for -m and both are specifying
			the photon moves per thread. Here, variable cfg->nphoton is 
			indeed the photon moves per thread.
		     */
		     case 'n':
				mcx_error(-2,"specifying photon number is not supported, please use -m");
		     	        i=mcx_readarg(argc,argv,i,&(cfg->nphoton),"int");
		     	        break;
		     case 'm':
		     	        i=mcx_readarg(argc,argv,i,&(cfg->nphoton),"int");
		     	        break;
		     case 't':
		     	        i=mcx_readarg(argc,argv,i,&(cfg->nthread),"int");
		     	        break;
                     case 'T':
                               	i=mcx_readarg(argc,argv,i,&(cfg->nblocksize),"int");
                               	break;
		     case 's':
		     	        i=mcx_readarg(argc,argv,i,cfg->session,"string");
		     	        break;
		     case 'a':
		     	        i=mcx_readarg(argc,argv,i,&(cfg->isrowmajor),"char");
		     	        break;
		     case 'g':
		     	        i=mcx_readarg(argc,argv,i,&(cfg->maxgate),"int");
		     	        break;
		     case 'b':
		     	        i=mcx_readarg(argc,argv,i,&(cfg->isreflect),"char");
		     	        break;
                     case 'B':
                                i=mcx_readarg(argc,argv,i,&(cfg->isref3),"char");
                               	break;
		     case 'd':
		     	        i=mcx_readarg(argc,argv,i,&(cfg->issavedet),"char");
		     	        break;
		     case 'r':
		     	        i=mcx_readarg(argc,argv,i,&(cfg->respin),"int");
		     	        break;
		     case 'S':
		     	        i=mcx_readarg(argc,argv,i,&(cfg->issave2pt),"char");
		     	        break;
		     case 'p':
		     	        i=mcx_readarg(argc,argv,i,&(cfg->printnum),"int");
		     	        break;
                     case 'e':
		     	        i=mcx_readarg(argc,argv,i,&(cfg->minenergy),"float");
                                break;
		     case 'U':
		     	        i=mcx_readarg(argc,argv,i,&(cfg->isnormalized),"char");
		     	        break;
                     case 'R':
                                i=mcx_readarg(argc,argv,i,&(cfg->sradius),"float");
                                break;
                     case 'l':
                                issavelog=1;
                                break;
		     case 'L':  
		                cfg->isgpuinfo=2;
		                break;
		     case 'I':  
		                cfg->isgpuinfo=1;
		                break;
		     case 'c':  
		                cfg->iscpu=1;
		                break;
		     case 'v':  
		                cfg->isverbose=1;
		                break;
		     case 'o':
		     	        i=mcx_readarg(argc,argv,i,cfg->rootpath,"string");
		     	        break;
		     case 'k': 
		     	        i=mcx_readarg(argc,argv,i,cfg->kernelfile,"string");
				break;
                     case 'D':
			        i=mcx_readarg(argc,argv,i,cfg->deviceid,"bytenumlist");
                                break;
                     case 'G':
                                i=mcx_readarg(argc,argv,i,cfg->deviceid,"string");
                                break;
                     case 'W':
			        i=mcx_readarg(argc,argv,i,cfg->workload,"floatlist");
                                break;
                     case 'z':
                                i=mcx_readarg(argc,argv,i,&(cfg->issrcfrom0),"char");
                                break;
		}
	    }
	    i++;
     }
     if(issavelog && cfg->session){
          sprintf(logfile,"%s.log",cfg->session);
          cfg->flog=fopen(logfile,"wt");
          if(cfg->flog==NULL){
		cfg->flog=stdout;
		fprintf(cfg->flog,"unable to save to log file, will print from stdout\n");
          }
     }
     if(cfg->clsource==NULL && cfg->isgpuinfo!=2){
     	  FILE *fp=fopen(cfg->kernelfile,"rb");
	  int srclen;
	  if(fp==NULL){
	  	mcx_error(-10,"the specified OpenCL kernel file does not exist!");
	  }
	  fseek(fp,0,SEEK_END);
	  srclen=ftell(fp);
	  cfg->clsource=(char *)malloc(srclen+1);
	  fseek(fp,0,SEEK_SET);
	  fread(cfg->clsource,1,srclen,fp);
	  cfg->clsource[srclen]='\0';
	  fclose(fp);
     }
     if(cfg->isgpuinfo!=2){ /*print gpu info only*/
       if(isinteractive){
          mcx_readconfig("",cfg);
       }else{
     	  mcx_readconfig(filename,cfg);
       }
     }
}

void mcx_usage(char *exename){
     printf("\
######################################################################################\n\
#                      Monte Carlo eXtreme (MCX) -- OpenCL                           #\n\
#              Author: Qianqian Fang <fangq at nmr.mgh.harvard.edu>                  #\n\
#                                                                                    #\n\
#      Martinos Center for Biomedical Imaging, Massachusetts General Hospital        #\n\
######################################################################################\n\
$MCX $Rev:: 155 $, Last Commit: $Date:: 2009-12-19 18:57:32 -05#$ by $Author:: fangq $\n\
######################################################################################\n\
\n\
usage: %s <param1> <param2> ...\n\
where possible parameters include (the first item in [] is the default value)\n\
 -i 	        (--interactive) interactive mode\n\
 -f config      (--input)	read config from a file\n\
 -t [1024|int]  (--thread)	total thread number\n\
 -T [128|int]   (--blocksize)	thread number per block\n\
 -m [0|int]     (--move)	total photon moves\n\
 -n [0|int]     (--photon)	total photon number (not supported yet, use -m only)\n\
 -r [1|int]     (--repeat)	number of repeations\n\
 -a [0|1]       (--array)	0 for Matlab array, 1 for C array\n\
 -z [0|1]       (--srcfrom0)    src/detector coordinates start from 0, otherwise from 1\n\
 -g [1|int]     (--gategroup)	number of time gates per run\n\
 -b [1|0]       (--reflect)	1 to reflect the photons at the boundary, 0 to exit\n\
 -B [0|1]       (--reflect3)	1 to consider maximum 3 reflections, 0 consider only 2\n\
 -e [0.|float]  (--minenergy)	minimum energy level to propagate a photon\n\
 -R [0.|float]  (--skipradius)  minimum distance to source to start accumulation\n\
 -U [1|0]       (--normalize)	1 to normailze the fluence to unitary, 0 to save raw fluence\n\
 -d [1|0]       (--savedet)	1 to save photon info at detectors, 0 not to save\n\
 -S [1|0]       (--save2pt)	1 to save the fluence field, 0 do not save\n\
 -s sessionid   (--session)	a string to identify this specific simulation (and output files)\n\
 -p [0|int]     (--printlen)	number of threads to print (debug)\n\
 -h             (--help)	print this message\n\
 -l             (--log) 	print messages to a log file instead\n\
 -L             (--listgpu)	print GPU information only\n\
 -I             (--printgpu)	print GPU information and run program\n\
 -c             (--cpu) 	use CPU as the platform for OpenCL backend\n\
 -k[mcx_core.cl](--kernel)      specify OpenCL kernel source\n\
 -G '012abc'    (--devicelist)  specify the list of OpenCL devices (CPU:0-9,GPU:a-zA-Z)\n\
 -W '50,30,20'  (--workload)    specify relative workload for each device; total is the sum\n\
example:\n\
       %s -t 1024 -T 256 -m 1000000 -f input.inp -s test -r 1 -a 0 -g 10 -U 0\n",exename,exename);
}
