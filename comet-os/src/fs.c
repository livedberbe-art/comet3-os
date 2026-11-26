#include "kernel.h"

fnode fsn[FS_MAX]; int fs_home,fs_pics,fs_docs,fs_desk,fs_dl,fs_apps,fs_trash;

int fs_find(int dir,const char*name){for(int i=1;i<FS_MAX;i++)if(fsn[i].used&&fsn[i].parent==dir&&!strcmp(fsn[i].name,name))return i;return -1;}
int fs_create(int dir,const char*name,int isdir){
    if(!name[0]||fs_find(dir,name)>=0)return -1;
    for(int i=1;i<FS_MAX;i++)if(!fsn[i].used){memset(&fsn[i],0,sizeof(fnode));fsn[i].used=1;fsn[i].parent=dir;fsn[i].dir=isdir;scopy(fsn[i].name,name,32);return i;}
    return -1;
}
int fs_resolve(int cwd,const char*p){
    int cur=p[0]=='/'?0:cwd;char tok[32];
    if(p[0]=='~'){cur=fs_home;p++;}
    while(*p){
        while(*p=='/')p++;if(!*p)break;int n=0;while(*p&&*p!='/'&&n<31)tok[n++]=*p++;tok[n]=0;
        if(!strcmp(tok,"."))continue;
        if(!strcmp(tok,"..")){if(cur!=0)cur=fsn[cur].parent;continue;}
        if(!fsn[cur].dir)return -1;cur=fs_find(cur,tok);if(cur<0)return -1;
    }
    return cur;
}
int fs_write(int n,const void*d,u32 len){
    if(n<=0||fsn[n].dir)return -1;
    if(fsn[n].data&&fsn[n].size>0&&fsn[n].size<=32768&&(len!=fsn[n].size||memcmp(fsn[n].data,d,len))){
        if(fsn[n].prev)kfree(fsn[n].prev);fsn[n].prev=kmalloc(fsn[n].size);if(fsn[n].prev){memcpy(fsn[n].prev,fsn[n].data,fsn[n].size);fsn[n].prevsz=fsn[n].size;}}
    if(len>fsn[n].cap||!fsn[n].data){u32 nc=len+256;u8*nb=kmalloc(nc);if(!nb)return -1;if(fsn[n].data)kfree(fsn[n].data);fsn[n].data=nb;fsn[n].cap=nc;}
    if(len)memcpy(fsn[n].data,d,len);fsn[n].size=len;return 0;
}
int fs_append(int n,const void*d,u32 len){
    if(n<=0||fsn[n].dir)return -1;u32 ns=fsn[n].size+len;
    if(ns>fsn[n].cap||!fsn[n].data){u32 nc=ns*2+256;u8*nb=kmalloc(nc);if(!nb)return -1;if(fsn[n].data){memcpy(nb,fsn[n].data,fsn[n].size);kfree(fsn[n].data);}fsn[n].data=nb;fsn[n].cap=nc;}
    memcpy(fsn[n].data+fsn[n].size,d,len);fsn[n].size=ns;return 0;
}
int fs_remove(int n){
    if(n<=0)return -1;
    if(fsn[n].dir)for(int i=1;i<FS_MAX;i++)if(fsn[i].used&&fsn[i].parent==n)fs_remove(i);
    if(fsn[n].data)kfree(fsn[n].data);if(fsn[n].prev)kfree(fsn[n].prev);fsn[n].prev=0;fsn[n].used=0;fsn[n].data=0;return 0;
}
void fs_path(int n,char*out,int max){
    if(n==0){scopy(out,"/",max);return;}
    int st[16],k=0;while(n>0&&k<16){st[k++]=n;n=fsn[n].parent;}
    int o=0;out[0]=0;while(k>0&&o<max-2){out[o++]='/';const char*s=fsn[st[--k]].name;while(*s&&o<max-1)out[o++]=*s++;}
    out[o]=0;
}
int fs_children(int dir,int*out,int max){
    int n=0;for(int i=1;i<FS_MAX&&n<max;i++)if(fsn[i].used&&fsn[i].parent==dir)out[n++]=i;
    for(int a=1;a<n;a++){int v=out[a],b=a-1;
        while(b>=0&&((fsn[out[b]].dir<fsn[v].dir)||(fsn[out[b]].dir==fsn[v].dir&&strcmp(fsn[out[b]].name,fsn[v].name)>0))){out[b+1]=out[b];b--;}
        out[b+1]=v;}
    return n;
}
u32 fs_used(void){u32 s=0;for(int i=1;i<FS_MAX;i++)if(fsn[i].used)s+=fsn[i].size;return s;}
int fs_nodes(void){int c=0;for(int i=1;i<FS_MAX;i++)c+=fsn[i].used;return c;}
int fs_rename(int n,const char*name){if(n<=0||!name[0]||fs_find(fsn[n].parent,name)>=0)return -1;scopy(fsn[n].name,name,32);return 0;}
int fs_unique(int dir,const char*base,char*out){
    scopy(out,base,32);for(int i=2;fs_find(dir,out)>=0&&i<100;i++)ksnprintf(out,32,"%s %d",base,i);return 0;
}
static int content_has(int i,const char*q){
    if(!fsn[i].data||fsn[i].size<2||fsn[i].size>65536||is_img_name(fsn[i].name))return 0;int ql=strlen(q);if(ql<2)return 0;const u8*d=fsn[i].data;
    for(u32 a=0;a+ql<=fsn[i].size;a++){int k=0;for(;k<ql;k++){int c=d[a+k],e=q[k];if(c>='A'&&c<='Z')c+=32;if(e>='A'&&e<='Z')e+=32;if(c!=e)break;}if(k==ql)return 1;}
    return 0;
}
int fs_search(const char*q,int*out,int max){int n=0;for(int i=1;i<FS_MAX&&n<max;i++)if(fsn[i].used&&!fsn[i].dir&&(stricontains(fsn[i].name,q)||content_has(i,q)))out[n++]=i;return n;}
int is_img_name(const char*n){return ends_with(n,".cim")||ends_with(n,".bmp")||ends_with(n,".png")||ends_with(n,".jpg")||ends_with(n,".jpeg");}
int fs_trash_node(int n){if(n<=0||fsn[n].parent==fs_trash)return fs_remove(n);char nm[32];fs_unique(fs_trash,fsn[n].name,nm);scopy(fsn[n].name,nm,32);fsn[n].oparent=fsn[n].parent;fsn[n].parent=fs_trash;return 0;}
int fs_untrash(int n){if(n<=0||fsn[n].parent!=fs_trash)return -1;int p=fsn[n].oparent;if(p<0||!fsn[p].used||!fsn[p].dir)p=fs_docs;char nm[32];fs_unique(p,fsn[n].name,nm);scopy(fsn[n].name,nm,32);fsn[n].parent=p;return 0;}
int fs_restore_prev(int n){if(n<=0||!fsn[n].prev)return -1;u8*cur=fsn[n].data;u32 cs=fsn[n].size;u8*cp=kmalloc(cs?cs:1);if(cp&&cs)memcpy(cp,cur,cs);u8*pv=fsn[n].prev;u32 ps=fsn[n].prevsz;fsn[n].prev=0;fs_write(n,pv,ps);kfree(pv);if(fsn[n].prev)kfree(fsn[n].prev);fsn[n].prev=cp;fsn[n].prevsz=cs;return 0;}

/* ------------------------------------------------ tar ------------------ */
static u32 oct_get(const u8*p,int n){u32 v=0;for(int i=0;i<n&&p[i]>='0'&&p[i]<='7';i++)v=v*8+(p[i]-'0');return v;}
static void oct_put(u8*p,u32 v,int digits){for(int i=digits-1;i>=0;i--){p[i]='0'+(v&7);v>>=3;}}
int tar_list(const u8*d,u32 len,tar_ent*out,int max){
    int n=0;u32 off=0;
    while(off+512<=len&&n<max){
        const u8*h=d+off;if(!h[0])break;
        scopy(out[n].name,(const char*)h,100);out[n].size=oct_get(h+124,11);out[n].dir=(h[156]=='5');n++;
        off+=512+((oct_get(h+124,11)+511)&~511u);
    }
    return n;
}
static int mkpath(int dest,const char*path,int isdir){
    char tok[32];int cur=dest;const char*p=path;
    while(*p){
        while(*p=='/')p++;if(!*p)break;int n=0;while(*p&&*p!='/'&&n<31)tok[n++]=*p++;tok[n]=0;
        int last=!*p||(*p=='/'&&!p[1]);int f=fs_find(cur,tok);
        if(f<0)f=fs_create(cur,tok,last?isdir:1);if(f<0)return -1;cur=f;
    }
    return cur;
}
int tar_extract(int tn,int dest){
    if(tn<=0||fsn[tn].dir||!fsn[tn].data)return -1;
    const u8*d=fsn[tn].data;u32 len=fsn[tn].size,off=0;int cnt=0;
    while(off+512<=len){
        const u8*h=d+off;if(!h[0])break;u32 sz=oct_get(h+124,11);char nm[100];scopy(nm,(const char*)h,100);
        int isd=h[156]=='5';int n=mkpath(dest,nm,isd);
        if(n>0&&!isd&&off+512+sz<=len){fs_write(n,d+off+512,sz);cnt++;}
        off+=512+((sz+511)&~511u);
    }
    return cnt;
}
static u32 tar_size(int n){
    if(!fsn[n].dir)return 512+((fsn[n].size+511)&~511u);
    u32 s=512;for(int i=1;i<FS_MAX;i++)if(fsn[i].used&&fsn[i].parent==n)s+=tar_size(i);return s;
}
static void tar_hdr(u8*h,const char*name,u32 size,int dir){
    memset(h,0,512);scopy((char*)h,name,100);
    memcpy(h+100,"0000644",8);memcpy(h+108,"0000000",8);memcpy(h+116,"0000000",8);
    oct_put(h+124,size,11);memcpy(h+136,"00000000000",12);h[156]=dir?'5':'0';memcpy(h+257,"ustar",6);memcpy(h+263,"00",2);
    memset(h+148,' ',8);u32 sum=0;for(int i=0;i<512;i++)sum+=h[i];oct_put(h+148,sum,6);h[154]=0;h[155]=' ';
}
static void tar_add(u8*buf,u32*off,int n,const char*prefix){
    char nm[100];ksnprintf(nm,100,"%s%s%s",prefix,fsn[n].name,fsn[n].dir?"/":"");
    tar_hdr(buf+*off,nm,fsn[n].dir?0:fsn[n].size,fsn[n].dir);*off+=512;
    if(!fsn[n].dir){if(fsn[n].size)memcpy(buf+*off,fsn[n].data,fsn[n].size);*off+=(fsn[n].size+511)&~511u;}
    else for(int i=1;i<FS_MAX;i++)if(fsn[i].used&&fsn[i].parent==n)tar_add(buf,off,i,nm);
}
int tar_create(int outn,int*nodes,int n){
    u32 tot=1024;for(int i=0;i<n;i++)tot+=tar_size(nodes[i]);
    u8*b=kzalloc(tot);if(!b)return -1;u32 off=0;
    for(int i=0;i<n;i++)tar_add(b,&off,nodes[i],"");
    int r=fs_write(outn,b,tot);kfree(b);return r;
}

/* ------------------------------------------------ images --------------- */
u32 *img_load(int n,int*w,int*h,int*owned){
    *owned=0;if(n<=0||fsn[n].dir||!fsn[n].data)return 0;const u8*d=fsn[n].data;u32 sz=fsn[n].size;
    if(sz>8&&!memcmp(d,"CIM1",4)){int iw=d[4]|(d[5]<<8),ih=d[6]|(d[7]<<8);if(sz<8+(u32)iw*ih*4)return 0;*w=iw;*h=ih;return (u32*)(d+8);}
    if(sz>40&&d[0]==0x89&&d[1]=='P'){u32*p=png_decode(d,sz,w,h);if(p)*owned=1;return p;}
    if(sz>4&&d[0]==0xFF&&d[1]==0xD8){u32*p=jpg_decode(d,sz,w,h);if(p)*owned=1;return p;}
    if(sz>54&&d[0]=='B'&&d[1]=='M'){
        u32 off=*(const u32*)(d+10);int iw=*(const int*)(d+18),ih=*(const int*)(d+22);int bpp=*(const u16*)(d+28);u32 comp=*(const u32*)(d+30);
        int flip=ih>0;if(ih<0)ih=-ih;if(comp||(bpp!=24&&bpp!=32)||iw<=0||ih<=0||iw>2048||ih>2048)return 0;
        u32 stride=((iw*bpp/8)+3)&~3u;if(off+stride*ih>sz)return 0;
        u32*px=kmalloc(iw*ih*4);if(!px)return 0;
        for(int y=0;y<ih;y++){const u8*row=d+off+stride*(flip?ih-1-y:y);for(int x=0;x<iw;x++){const u8*p=row+x*(bpp/8);px[y*iw+x]=(p[2]<<16)|(p[1]<<8)|p[0];}}
        *w=iw;*h=ih;*owned=1;return px;
    }
    return 0;
}
int img_save_cim(int dir,const char*name,const u32*px,int w,int h){
    char nm[32];fs_unique(dir,name,nm);int n=fs_create(dir,nm,0);if(n<0)return -1;
    u32 tot=8+w*h*4;u8*b=kmalloc(tot);if(!b)return -1;memcpy(b,"CIM1",4);b[4]=w;b[5]=w>>8;b[6]=h;b[7]=h>>8;memcpy(b+8,px,w*h*4);
    int r=fs_write(n,b,tot);kfree(b);return r;
}
static void gen_image(const char*name,int kind){
    int w=320,h=200;u32*px=kmalloc(w*h*4);if(!px)return;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        u32 c=0;
        if(kind==0){
            c=blend(0x050A1E,0x0B3B4A,y*255/h);int cy=h*40/100+isin(x*3)*h/12/1024+isin(x*7+40)*h/40/1024,dd=ABS(y-cy),band=h/5;
            if(dd<band){int a=(band-dd)*255/band;a=a*a/255;c=blend(c,blend(0x34D399,0x8B5CF6,x*255/w),a*7/10);}
            if(((x*73+y*151)%997)==0)c=0xFFFFFF;
        }else if(kind==1){
            int v=isin(x*4)+isin(y*5)+isin((x+y)*3)+isin((int)isqrt(x*x+y*y)*6);int i=(v+4096)*255/8192;
            int r=(isin(i*3)+1024)*255/2048,g=(isin(i*3+120)+1024)*255/2048,b=(isin(i*3+240)+1024)*255/2048;c=(r<<16)|(g<<8)|b;
        }else if(kind==2){
            c=0x05060F;int d1=(x-90)*(x-90)+(y-80)*(y-80),d2=(x-230)*(x-230)+(y-110)*(y-110),d3=(x-160)*(x-160)+(y-40)*(y-40);
            if(d1<9000)c=blend(c,0xEC4899,(9000-d1)*200/9000);if(d2<12000)c=blend(c,0x6366F1,(12000-d2)*210/12000);if(d3<6000)c=blend(c,0x22D3EE,(6000-d3)*150/6000);
            if(((x*131+y*197+x*y)%523)==0)c=0xFFFFFF;
        }else{
            int cr=-10240+x*14336/w,ci=-5120+y*10240/h,zr=0,zi=0,it=0;
            while(it<48){int a=(zr*zr)>>12,b=(zi*zi)>>12;if(a+b>16384)break;zi=((zr*zi)>>11)+ci;zr=a-b+cr;it++;}
            c=it==48?0x05060F:blend(0x0B1033,0x7DD3FC,it*255/48);if(it<48&&it>8)c=blend(c,0xF472B6,(it-8)*4);
        }
        px[y*w+x]=c;
    }
    img_save_cim(fs_pics,name,px,w,h);kfree(px);
}
static void put_text(int dir,const char*name,const char*txt){int n=fs_create(dir,name,0);if(n>0)fs_write(n,txt,strlen(txt));}

void fs_init(void){
    memset(fsn,0,sizeof fsn);fsn[0].used=1;fsn[0].dir=1;fsn[0].parent=-1;
    int home=fs_create(0,"home",1),guest=fs_create(home,"guest",1);fs_home=guest;
    fs_desk=fs_create(guest,"Desktop",1);fs_docs=fs_create(guest,"Documents",1);fs_pics=fs_create(guest,"Pictures",1);
    fs_apps=fs_create(guest,"Apps",1);fs_trash=fs_create(0,"trash",1);int mus=fs_create(guest,"Music",1);fs_dl=fs_create(guest,"Downloads",1);int etc=fs_create(0,"etc",1);int sys=fs_create(0,"system",1);(void)mus;
    put_text(etc,"os-release","NAME=\"Comet OS\"\nVERSION=\"1.0 (Perseid)\"\nID=comet\nHOME_URL=\"about:comet\"\n");
    put_text(etc,"hostname","comet\n");
    put_text(sys,"kernel.log","[ok] gdt/idt/pic/pit\n[ok] framebuffer compositor\n[ok] ps/2 keyboard + mouse\n[ok] ramfs mounted at /\n[ok] ata/atapi probe\n");
    put_text(fs_desk,"readme.txt","Drop things here. Right-click the desktop for options.\n");
    fs_docs=fs_find(guest,"Documents");
    put_text(fs_docs,"welcome.txt",
        "Welcome to Comet OS 1.0 \"Perseid\"\n\n"
        "Comet OS is a from-scratch desktop operating system:\n"
        "a 32-bit kernel, a software compositor and a small set of apps.\n\n"
        "Shortcuts\n"
        "  Win              Start menu (just type to search)\n"
        "  Win+Tab          Task View / virtual desktops\n"
        "  Win+D            Show desktop\n"
        "  Win+Arrows       Snap windows (Win+Up maximizes)\n"
        "  Ctrl+Win+Left/Right  Switch virtual desktop\n"
        "  Alt+Tab          Switch windows\n"
        "  Alt+F4           Close window\n"
        "  Ctrl+Shift+Esc   Task Manager\n"
        "  Win+A / Win+W    Quick settings / Widgets\n\n"
        "Hover the maximize button for snap layouts, or drag a window\n"
        "to a screen edge. Open the Terminal and type 'help'.\n");
    put_text(fs_docs,"notes.txt","My notes\n--------\n\nClick here and start typing. Ctrl+S saves.\n");
    put_text(fs_docs,"todo.md","# To do\n- [x] boot Comet OS\n- [ ] try snap layouts\n- [ ] install to disk\n- [ ] beat the high score in Comet Rush\n");
    int sd=fs_create(fs_dl,"samples",1);put_text(sd,"hello.c","#include <stdio.h>\nint main(void){puts(\"hello from a comet\");return 0;}\n");put_text(sd,"data.csv","name,mass\nhalley,2.2e14\nencke,9.2e13\n");
    int pk[3]={fs_find(fs_docs,"welcome.txt"),fs_find(fs_docs,"todo.md"),sd};
    int tn=fs_create(fs_dl,"comet-starter.tar",0);tar_create(tn,pk,3);

    put_text(fs_apps,"counter.cs","title Click Counter\nlet n = 0\non click\nlet n = n + 1\non draw\nfill bg\ntext 40 40 \"Clicks: $n\" text 3\nrrect 40 100 220 56 14 accent\ntext 62 122 \"Click anywhere\" white 1\n");
    put_text(fs_apps,"bounce.cs","title Bouncing Ball\nlet x = 100\nlet y = 60\nlet a = 3\nlet b = 2\non tick\nlet x = x + a\nlet y = y + b\nif x < 12 || x > cw - 12\nlet a = -a\nend\nif y < 12 || y > ch - 12\nlet b = -b\nend\non draw\nfill 0x0B1020\ncircle x y 14 accent\ntext 12 12 \"Bouncing ball - a CometScript app\" white 1\n");
    put_text(fs_apps,"catch.cs","title Catch the Star\nlet p = 100\nlet x = 120\nlet y = 20\nlet s = 0\nlet v = 3\non tick\nlet p = mx - 40\nlet y = y + v\nif y > ch - 40 && y < ch - 14 && x > p - 6 && x < p + 86\nlet s = s + 1\nlet y = 0\nlet x = rand(cw - 40) + 20\nlet v = 3 + s / 4\nend\nif y > ch\nlet y = 0\nlet x = rand(cw - 40) + 20\nlet s = 0\nlet v = 3\nend\non draw\nfill 0x0B1020\nrrect p ch-30 80 12 6 accent\ncircle x y 9 yellow\ntext 12 12 \"Score: $s  (move the mouse)\" white 1\n");
    put_text(fs_apps,"README.txt","CometScript apps live here.\nOpen a .cs file to run it, or edit it in Notes and press Run.\nCommands: let fill rect rrect circle line text say title beep if/else/end while/end\nHandlers: on draw, on tick, on click, on key\nBuilt-ins: mx my cw ch key ticks rand(n) abs(n) min(a,b) max(a,b); colors: red green blue white black yellow orange pink purple gray cyan accent text bg\nVariables are single letters a-z. Use $a inside quotes to print a variable.\n");
    gen_image("aurora.cim",0);gen_image("plasma.cim",1);gen_image("nebula.cim",2);gen_image("mandelbrot.cim",3);
    {int bw=64,bh=48;u32 st=(bw*3+3)&~3u,tot=54+st*bh;u8*b=kzalloc(tot);
        if(b){b[0]='B';b[1]='M';*(u32*)(b+2)=tot;*(u32*)(b+10)=54;*(u32*)(b+14)=40;*(int*)(b+18)=bw;*(int*)(b+22)=bh;*(u16*)(b+26)=1;*(u16*)(b+28)=24;*(u32*)(b+34)=st*bh;
            for(int y=0;y<bh;y++)for(int x=0;x<bw;x++){u8*p=b+54+st*y+x*3;p[0]=255-y*4;p[1]=x*4;p[2]=y*5;}
            int n=fs_create(fs_pics,"gradient.bmp",0);fs_write(n,b,tot);kfree(b);}}
}
