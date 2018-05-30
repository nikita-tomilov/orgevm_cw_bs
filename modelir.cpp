//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop
#include <stdio.h>
#include <string.h>
#include "modelir.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm1 *Form1;
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent *Owner) : TForm(Owner) {}
//-----------------------------------------------------------------
DWORD bufd;
char str_hex[40];

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
//���������� ������� -----------------------------
DWORD bri, ii;
OVERLAPPED ovr, ovrd;
HANDLE hCom, hdisk, hdisk1, htempl = 0;
DCB dcb;
char coma;
char priznak; //�������!=0 ���������� ����
char cod;

// ����������� mcs51==================================
//����������� ������-------------------------

uchar IR, //������� ������
    Wrk,  //������� �������
    ACC,  //����������� � ������ ��������
    SP,   //��������� �����
    PA,   //������� � RALU
    PB,   //������� � RALU � �����
    PSW,  //����� ���������
    B,    //�������
    Ram[256], Xdata[2048], DPH, DPL, P0, P1, P2,
    P3 = 0xff, //������� ����� �3
    TCON, // tf1 tr1 tf0 tr0 ie1 it1 ie0 it0
    IE, // EA . .  es et1 ex1 et0 ex0
    IP,
    IEE,  //����� �������� IE
    vect; //������� ������� ����������;

//������ ������� ��������� � SFR--------------------
const Sp = 0x81; // wrsp - 1
const Acc = 0xe0; // wracc - 2
const Dph = 0x83; // wrdph  -3
const Dpl = 0x82; // wrdpl  -4
const b = 0xf0; // wrb  - 5
const Psw = 0xd0; // wrpsw - 6
const p3 = 0xb0; // wrp3 - 7
const p0 = 0x80; // wrp0 - 8
const p1 = 0x90; // wrp1 - 9
const p2 = 0xa0; // wrp2 - 10
const Ie = 0xa8; // wrie - 11
const Ip = 0xb8; // wrip - 12
const Tcon = 0x88; // wrtcon -13

char adress(char *ss) {
  char numm;
  if (strcmp(ss, "Acc") == 0)
    return numm = Acc;
  if (strcmp(ss, "Sp") == 0)
    return numm = Sp;
  if (strcmp(ss, "Dph") == 0)
    return numm = Dph;
  if (strcmp(ss, "Dpl") == 0)
    return numm = Dpl;
  if (strcmp(ss, "Psw") == 0)
    return numm = Psw;
  if (strcmp(ss, "p3") == 0)
    return numm = p3;
  if (strcmp(ss, "p0") == 0)
    return numm = p0;
  if (strcmp(ss, "p1") == 0)
    return numm = p1;
  if (strcmp(ss, "p2") == 0)
    return numm = p2;
  if (strcmp(ss, "Tcon") == 0)
    return numm = Tcon;
  if (strcmp(ss, "Ie") == 0)
    return numm = Ie;
  if (strcmp(ss, "Ip") == 0)
    return numm = Ip;
  if (strcmp(ss, "b") == 0)
    return numm = b;

  return 0;
}

uint RAMK; //������� ������� �����������
uint PC; // PCH=PC>>8,PCL=PC
uint DPTR; // DPH=DPTR>>8,DPL=DPTR
uint ROMM[128]; //������ ������������ - (���� <10) + 3���� <128

uchar WRSFR[128];

//��������� ���������� (����)----------------------
bool EA, EX0, EX1, //����� ����������
    IE0, IE1,      //������� ����������
    intr0, intr1, //�������� ����� ����������� ����������
    intra, intra2, //��� - ������� ���������� � ��� �������
    Cc,            //���� �������� � RALU
    W7;            //���� wrk[7]

// ulong codDCM; //32-��� ��� ������������ � �����
uchar CODE[2048]; //���� � ����� ������ + ������� �������� � ������

uchar ADC[0x100]; //�������������� ���� ������� � ����� ������ ��������������

//===========================����������� ��

//==========================����� ������� �����������
uchar *stro = "      "; //����� ������ ��� ������ ���������
uchar prizn;            //������� ��� ��������� �����

char CodMo(char *ss, char *sss) { //����� ��� ������������� *sss � ������ *ss
  char locs[5];
  char x;
  char *namemo = "      ";
  char *namemo1 = "        ";
  namemo1 = namemo;
  cod = 1;
  for (char ii = 0;; ii++) {
    x = *ss;
    ss++;
    if (((x == ',') || (x == ' ')) && ((strcmp(namemo1, sss) == 0))) {
      priznak = 1;
      return cod;
    }
    if (x == ',') {
      cod++;
      namemo = namemo1;
    }
    if (x == ' ')
      goto exita;
    // StrCat(itoa(PC,&locs[0],16),ss);

    if (x != ',') {
      *namemo++ = x;
      *namemo = 0;
    }
  }
exita:
  cod = 0; //������ ������� � ���� �������������
  return cod;
}

//������ ��������� ����������� �� ������

uint DCM1[64]; // MCROM[0x100];
uchar DCM8[64], DCM2[64], DCM3[64];
unsigned long DCM4[64], DCM7[64];

//���� ������� ��  i.code
uint CodDCM1;  // ��� 7  16mem
uint CodDCM2;  //  6  reg8
uint CodDCM3;  //  7 bita
ulong CodDCM4; //  19  ralu
uint CodDCM6;  //  7  interrupt
ulong CodDCM7; //  17 bus8
uint CodDCM8;  //  3 control
//  4 contr

uchar i1; //������� �����, ��������������� � DCM[i]

//����������� ����������� �� ������ ==========================

// 1 ���� 16mem===(DCM 1)===============================================

struct mik16 { //��������� ������������ ���������� ������ 16mem
  uchar selpc; // 3
  uchar unicod16; // 6
  uchar selaxa; // 1
} mk16; // 7 bit ������ ��
//����������� ����� ������������
//����� ������ - ������, ������ ����� - ���������
// char *QQQ ="   ";
char *Selpc = "Acall,Pc+a,Vect,Call ";
//             0     1    2    3
char *Unicod16 = "Wrpc,Wrxda,Incdptr,Incpc,Wrreg,Rdreg ";
//��� �������������   1     2     3     4
//��� 0 - ��� �������������
char *Selaxa = "Dptr,P2wrk ";
//    0     1

void Newmk16(void) {
  mk16.selpc = 0;
  mk16.unicod16 = 0;
}

void TakeCode1(char *stpole, char *stmo)  { //������������ ���� � ����� *stpole  ��� ������������� *stmo
  if (strcmp(stpole, "Selpc") == 0)
    mk16.selpc = CodMo(Selpc, stmo) - 1;
  else if (strcmp(stpole, "Selaxa") == 0)
    mk16.selaxa = CodMo(Selaxa, stmo) - 1;
  else if (strcmp(stpole, "Unicod16") == 0)
    mk16.unicod16 |= 1 << (CodMo(Unicod16, stmo) - 1);
}

// 3 ���� BIT====(DCM 3)==============================================
char bita;
char *Unibit = "Bitor,Bitand,Nebit,Newbit,Setb,Wlocpsw,Selpsw,Clrb,Movcb ";
//                 1     2      3     4     5      6      7     8     9    bit

void TakeCode3(char *stpole, char *stmo) {
  if (strcmp(stpole, "Unibit") == 0)
    bita |= 1 << (CodMo(Unibit, stmo) - 1);
}
// 4 ���� RALU=====(DCM 4)=============================================
struct ralu {
  uchar mop; // 3
  uchar selacc; // 3
  uchar selb; // 2
  uchar uniralu; // 10
  uchar selpa; // 1
  uchar selpb; // 2
} alu; // 20

char *Mop = "L,Suba,Subb,Add,Or,Xor,And,H ";
//      0  1    2    3  4  5    6  7      bit 3
char *Selacc = "Busb,M[7.0],Quot,Lac,Rac,F,Xdata ";
//          0      1    2   3    4  5    6       bit 3
char *SelB = "Busb,M[15.8],Remain,Wrk ";
//         0      1      2    3             bit 2
char *Uniralu = "Wrbloc,Wrpb,Wrpa,Ci,Mul,Div,Wrwrk,Incwrk,Count,Selpa ";
//             1     2    3   4   5   6    7     8     9     10      bit 10
char *Selpb = "L,Pcl,Dapb";
//            0   1     2         bit 2
char *Selpa = "Pa,Acc";
//     0   1

void Newalu(void) {
  alu.mop = 0;
  alu.uniralu = 0;
  alu.selb = 0;
  alu.selacc = 0;
  alu.selpa = 0;
  alu.selpb = 0;
}

void TakeCode4(char *stpole, char *stmo) { //������������ ���� � ����� *stpole  ��� ������������� *stmo

  if (strcmp(stpole, "Mop") == 0)
    alu.mop = CodMo(Mop, stmo);
  else if (strcmp(stpole, "Selacc") == 0)
    alu.selacc = CodMo(Selacc, stmo) - 1;
  else if (strcmp(stpole, "SelB") == 0)
    alu.selb = CodMo(SelB, stmo) - 1;
  else if (strcmp(stpole, "Uniralu") == 0)
    alu.uniralu = alu.uniralu | (1 << (CodMo(Uniralu, stmo) - 1));
  else if (strcmp(stpole, "Selpa") == 0)
    alu.selpa = CodMo(Selpa, stmo) - 1;
  else if (strcmp(stpole, "Selpb") == 0)
    alu.selpb = CodMo(Selpb, stmo) - 1;
}

// 4 ���� Ports======(DCM 2 )============================================

struct mk8 { // ports
  uchar adpi; // 2
  uchar adpin; // 2
  uchar uniport; // 5
} mkport; // 9 bit

char *Uniport = "Wrpi,Xadr,Altf2,Altf3,Xdata ";
//        1    2    3      4     5        bit 5
char *Adpi = "Rp0,Rp1,Rp2,Rp3";
//          0     1    2    3         bit 2
char *Adpin = "Pp0,Pp1,Pp2,Pp3";
//        0   1    2    3        bit 2

void TakeCode2(char *stpole, char *stmo) //��������� ������������ ���� ��������� ��������
{
  if (strcmp(stpole, "Uniport") == 0)
    mkport.uniport |= 1 << (CodMo(Uniport, stmo) - 1);
  else if (strcmp(stpole, "Adpin") == 0)
    mkport.adpin |= 1 << (CodMo(Adpin, stmo) - 1);
  else if (strcmp(stpole, "Adpi") == 0)
    mkport.adpi |= 1 << (CodMo(Adpi, stmo) - 1);
}

// 5 ���� Interrupt==================================================
//���������� � Contr

//����6   bus8  (dcm7 )
//========================================================
//����������� ������ ������� ��������� ��� ��������� � ������� � SFR
struct BUSS8 { //��������� ������������ ���������� ������ ralu
  uchar selbusb; // 4
  uchar selbusa; // 3
  uchar asfr; // 7
  uchar unibus8; // 4
} mkbus8; // 18 ���
char *SelbusB = "Com,Wrk,Pch,Pcl,Pinpi,Rdpi,Nwtcon,Acc,Ram,Ff,Psw ";
//              0  1   2   3     4    5      6    7  8  9  10   4 ���
char *SelbusA = "Riram,Wrk,Asfr,Sp,Abitwrk ";
//                0    1   2   3    4      3 ���
char *Unibus8 = "Ari,Wram,Selsp,Incsp ";
//              1    2    3     4           4 ���
// adsfr - ������ ����� � �������� � ������� ����������
char *Asfr = "Acc,Sp,B,Psw,Dpl,Dph,P0,P1,P2,P3,Tcon,Ip,Ie ";

void Newbus8(void) {
  mkbus8.selbusb = 0;
  mkbus8.selbusa = 0;
  mkbus8.asfr = 0;
  mkbus8.unibus8 = 0;
}

void TakeCode7(char *stpole, char *stmo) { //������������ ���� � ����� *stpole  ��� ������������� *stmo

  if (strcmp(stpole, "SelbusA") == 0)
    mkbus8.selbusa = CodMo(SelbusA, stmo) - 1;
  else if (strcmp(stpole, "SelbusB") == 0)
    mkbus8.selbusb = CodMo(SelbusB, stmo) - 1;
  else if (strcmp(stpole, "Asfr") == 0) {
    mkbus8.asfr = *stmo & 0x7f;
    priznak = 1;
  } else if (strcmp(stpole, "Unibus8") == 0)
    mkbus8.unibus8 |= 1 << (CodMo(Unibus8, stmo) - 1);
}
//����7   Control  (dcm8.mif )  + interrupt
//========================================================
//����������� ������ ������� ��������� ��� ��������� � ������� � SFR
uchar unicontr;
char *Unicon = "Clramk,Ramk1,Wrir,Eintra,Clreq,Clrinta ";
//                   1      2     3   4     5      6
void TakeCode8(char *stpole, char *stmo) {
  if (strcmp(stpole, "Unicont") == 0)
    unicontr = 1 << (CodMo(Unicon, stmo) - 1);
}

//����   ROMM  (DCM9.mif )
//========================================================
//����������� ������ ������� ��������� ��� ��������� � ������� � SFR
uint ifromm; // Selif,Neiff,dcrom)
char *Selif = "L,Cc,Zacc,Bitwrk,Wrk7,PswC,Intra";
//            0 1    2      3    4   5     6          3 ���

void Takeromm(
    char *stpole,
    char *stmo) { //������������ ���� � ����� *stpole  ��� ������������� *stmo

  if (strcmp(stpole, "Selif") == 0)
    ifromm |= CodMo(Selif, stmo);
  if (strcmp(stmo, "Neiff")) //����� �������� ����
    ifromm |= 0x80;
}

//================����������� ����������� �� ������
void __fastcall TForm1::TakeCode(
    char *stpole,
    char *stmo) { //������������ ����� � ����� *stpole  ��� ������������� *stmo
  priznak = 0;
  TakeCode1(stpole, stmo); // mem16
  TakeCode2(stpole, stmo); // ports
  TakeCode3(stpole, stmo); // bita
  TakeCode4(stpole, stmo); // ralu
  TakeCode7(stpole, stmo); // bus8
  TakeCode8(stpole, stmo); // Control
  Takeromm(stpole, stmo); // ROMM
  if (priznak)
    return; //
  Edit8->Text = "������ ����"; // StrCat("PC=",ss);
}
void EqualCode(void) {
  char priznak; //��������� ��������������� ����
  for (char i = 0; i < i1; i++) {
    priznak = 0;
    if (DCM1[i] != CodDCM1) {
      priznak = 1;
      continue;
    } //������������ �����;
    if (DCM2[i] != CodDCM2) {
      priznak = 1;
      continue;
    };
    if (DCM3[i] != CodDCM3) {
      priznak = 1;
      continue;
    };
    if (DCM4[i] != CodDCM4) {
      priznak = 1;
      continue;
    };
    if (DCM7[i] != CodDCM7) {
      priznak = 1;
      continue;
    };
    if (DCM8[i] != CodDCM8) {
      priznak = 1;
      continue;
    };
    if (priznak == 0)
      ROMM[RAMK] = i | (ifromm << 6);
    break;
  }
  if (priznak == 1) //���������� ������ ����
  {
    DCM1[i1] = CodDCM1; // mem16
    DCM2[i1] = CodDCM2; // reg8
    DCM3[i1] = CodDCM3; // bita
    DCM4[i1] = CodDCM4; // ralu
    DCM7[i1] = CodDCM7; // bus8
    DCM8[i1] = CodDCM8; // Control
    //����� ��
    i1++;
  }
}

void CompareCode(void) //������������ ����� ����������� �� ������
{
  CodDCM1 = (((mk16.selpc << 1) | mk16.selaxa) << 4) | mk16.unicod16;

  // CodDCM2=mkporta;

  // CodDCM3=(bita.selpsw<<6)|bita.unibit;

  //CodDCM4= (((((((((alu.mop <<3)|alu.selacc)<<2)\
      //                  |alu.selb)<<1)|alu.selpa)<<1)|\
        //                       alu.selpb)<<9)|alu.uniralu;

  CodDCM7 =
      (((((mkbus8.selbusb << 3) | mkbus8.selbusa) << 7) | mkbus8.asfr) << 3) |
      mkbus8.unibus8;

  CodDCM8 = unicontr;
  //���������� ����� �����������
  //��������� � ���������� �������������� �� ������
  // 0-������������ =0
  ROMM[RAMK] = ifromm << 6; //+dcmicro

  EqualCode();
}

char ss[10];
void __fastcall TForm1::MicroCodMem(char *ss) //�����������  ������������
{ //*ss -���������� ������ ������������
  if (CheckBox1->State == cbChecked) //���������� �����������
  {
    uchar *namepole = "          "; //��� ���� �������������
    uchar *namemo = "          ";   //��� �������������
    uchar x, xx = ','; //�������� ������������ �������
    uchar *namepole1 = "          ";
    uchar *namemo1 = "          "; //���������, ����������� �����
    namepole1 = namepole; //���������� ����������
    namemo1 = namemo;
    //����� ��������
    // Newmk16();  Newalu();  Newbus8(); Newbita();
    // ifromm=0; unicontr=0;  mkporta=0;

    {
      for (char ii = 0;; ii++) {
        x = *ss;
        ss++;
        if (xx == '=') //������ ����� -��� �������������
          if ((x == ',') || (x == ' ')) //����� �������������
          {
            TakeCode(namepole1, namemo1);
            namepole = namepole1;
            namemo = namemo1;
            for (char i = 0; i < 10; i++) // 7 ��������
              *namemo++ = *namepole++ = 0; //' ';
            namepole = namepole1;
            namemo = namemo1;
            xx = x;
            if (x == ' ')
              goto finn; //����� ������������
          } else {
            *namemo++ = x;
            *namemo = 0;
          } //������� ����� �������������
        else // xx=',' ������� ����� ���� �������������
            if (x == '=')
          xx = '='; //������������ �� ������� ����� �������������
        else {
          *namepole++ = x;
          *namepole = 0;
        }
      }
    finn:
      CompareCode(); //���������� ������� �����������
                     //������������ � �������
    }                //� �����������
  }
}

//=============================== ������������� ����

void __fastcall TForm1::GoToInt(void) //����� ����� � ���������� //+ 0-��������������
{
  // if(CheckBox9->State==cbUnchecked)

  if (!intra2) {
    if (IE0) // IE0 -��� ������� � TCON
    {
      intr0 = EX0 & ((TCON & 0x2) != 0) & EA; //������������
      CheckBox5->State = cbChecked;
      P3 ^= 0x4;
    } else if (IE1) {
      intr1 = EX1 & ((TCON & 0x8) != 0) & EA & !intr0;
      CheckBox6->State = cbChecked;
      P3 ^= 0x8;
    }
    intra = intr0 | intr1;
    if (intra)
      intra2 = 1; //���������� ������� ����� � ����������
    if (intr0) {
      vect = 0x3;
      intr0 = 0;
      IE0 = 0;
      CheckBox5->State = cbUnchecked;
    }
    if (intr1) {
      vect = 0x13;
      intr1 = 0;
      IE1 = 0;
      CheckBox6->State = cbUnchecked;
    }
    if (intra) //���������������� ����������
      CheckBox9->State = cbChecked; //��������������� ������� �� ������
  }
}

void __fastcall TForm1::ClearInt(void) {
  /* if(CheckBox5->State!=cbUnchecked)
        {IE0=0; TCON&=0xFD; Ram[Tcon]=TCON;
        intr0=0; //������� �������
        CheckBox5->State=cbUnchecked;
        P3|=0x4;
     //����� ������� Int0 � Tcon ����� ����� � ����������
      }
    if(CheckBox6->State!=cbUnchecked)
   {//����� ������� Int1 � Tcon ����� ����� � ����������
     IE1=0; TCON&=0xF7; Ram[Tcon]=TCON;
       intr1=0; //������� �������
         CheckBox6->State=cbUnchecked;
         P3|=0x8;
         }
         */
}

//---------------------- �������� � ���� �������

char odd(void) //������������ �������� �������� P
{
  char yy;
  char x0 = ACC & 1;
  char x1 = ACC & 2;
  char x2 = ACC & 4;
  char x3 = ACC & 8;
  char x4 = ACC & 0x10;
  char x5 = ACC & 0x20;
  char x6 = ACC & 0x40;
  char x7 = ACC & 0x80;
  yy = (x0 != 0) ^ (x1 != 0) ^ (x2 != 0) ^ (x3 != 0) ^ (x4 != 0) ^ (x5 != 0) ^
       (x6 != 0) ^ (x7 != 0);
  return yy;
}

void __fastcall TForm1::PSWC(char *simv) //������������ ����� ���������
{
  int AB = 0;
  AnsiString Q, S;

  S.printf("%s", simv);
  Q.printf("add  "); //�������� 7
  if (S == Q) {
    AB = PA + PB;
    PSW = (AB & 0x100) ? PSW | 0x80 : PSW & 0x7f; // C
    PSW = ((~(PA ^ PB) & 0x80) & (PA ^ AB)) ? PSW | 0x04 : PSW & 0xfb; // OV
  }

  Q.printf("subb  ");
  if (S == Q) {
    AB = PA - PB - (PSW >> 7);
    PSW = (AB & 0x100) ? PSW & 0x7f : PSW | 0x80; //����=��C
    PSW = ((~(PA ^ (~PB)) & 0x80) & (PA ^ AB)) ? PSW | 0x04 : PSW & 0xfb; // OV
  }

  Q.printf("andc "); //�������� 7
  if (S == Q)
    PSW = (PB & (1 << (Wrk & 0x7))) ? PSW & 0xff : PSW & 0x7f; // c=c&bit

  (odd()) ? PSW |= 1 : PSW &= 0xfe; // P
}

//==========================������ ���������� �����
void __fastcall TForm1::Reg(char i) //����� � ����
{
  char Q[6];
  Q[0] = 'R';
  Q[1] = 0x30 + i;
  Q[2] = '=';
  Q[3] = 0;
  if (Ram[i])
    ComboBox2->Items->Add(StrCat(Q, itoa(Ram[i], stro, 16)));
}
void __fastcall TForm1::Stack(int i) {
  char Q[8];
  for (char j = 0; j < 8; j++)
    Q[j] = 0;
  Q[0] = 'D';
  Q[1] = '[';
  Q[2] = (i <= 9) ? 0x30 + i : 0x57 + i;
  Q[3] = ']';
  Q[4] = '=';
  Q[5] = 0;
  ComboBox6->Items->Add(StrCat(Q, itoa(Ram[i], stro, 16)));
}
char Q[9];
void __fastcall TForm1::bitreg(unsigned char var) {
  Q[8] = 0;
  uchar j = 0x80;
  for (char i = 0; i < 8; i++) {
    Q[i] = (var & j) ? '1' : '0';
    j = j >> 1;
  }
}

void __fastcall TForm1::StateMCU(void) { //������� ��������� ������

  // uchar i,j=0x80;
  Acu->Text = itoa(ACC, stro, 16);
  Work->Text = itoa(Wrk, stro, 16);
  ProgCnt->Text = itoa(PC, stro, 16);
  bitreg(PSW); //�������������� � ������� ������
  Edit1->Text = &Q[0]; //����� ������� ������ PSW
  bitreg(P3);
  Edit2->Text = &Q[0];
  W7 = (Wrk & 0x80) ? 1 : 0; // Wrk[7]
}

uchar vector[0x10];
void __fastcall TForm1::Button1Click(TObject *Sender) { //������� �������  �����
  uchar i;
  int k;
  // ������ �������� ==========================
  vector[0x1] = 0x03; // int0
  vector[0x2] = 0x0b; // tf0
  vector[0x4] = 0x13; // int1
  vector[0x8] = 0x1b; // tf1
  vector[0x10] = 0x23; // T1 v R1
  // ������� �������� ������ � �������� Sfr

  //--------------------------------------------
  for (k = 0; k < 0x100; k++) //����� �������� ������
    ADC[k] = 0;
  // uchar j=0;  //������� NOP
  //��������� ����� j-�� ��������������  RAMM=(j<<3).000, j=ADC[IR]

  ADC[0] = 0; //������� NOP-->0

  ADC[0x02] = 1; // ������� ljamp ad16
  for (i = 0x08; i <= 0x0f; i++)
    ADC[i] = 2;  // inc ri
  ADC[0x05] = 3; // inc ad
  ADC[0xa3] = 4; // inc dptr
  ADC[0xf4] = 5; // cpl a
  ADC[0xb6] = 6;
  ADC[0xb7] = 6; // xchd a, @ri
  ADC[0x12] = 7; // lcall ad16
  ADC[0x32] = 8; // reti

  //����� ���������������� ������ � ���������
  //---------------------------------------------
  for (k = 0; k < 128; k++)
    ROMM[k] = 0;
  //===================��������� ��������� ������� � ������� ���������
  for (int i = 0; i < 8; i++) {
    Ram[i] = 0;
  }

  Ram[Sp] = SP = 07;
  for (int i = SP; i < SP + 5; i++) {
    Ram[i] = 0;
  }
  Ram[0] = 0x11; //�������� R0
  Ram[1] = 0x01; //�������� R1
  Ram[Acc] = ACC = 0x82;
  Ram[Psw] = PSW = 0x80;
  Ram[Tcon] = TCON = 0;
  P0 = P1 = P2 = P3 = 0xff; //��������� ��������� ������
                            //����� ��������� ����������
                            //---------------------------------------------
  for (i = 0; i < 16; i++)
    DCM1[i] = DCM2[i] = DCM3[i] = DCM4[i] = DCM7[i] = DCM8[i] = 0;

  //=============================== ����������� ��� �����

  /*
    LOC  OBJ            LINE     SOURCE
    ----                   1     Cseg at 0h;
    0000 020006            2             ljmp _start
    0003 6100              3             var: db "a", 0
                           4
    0005                   5     _stuff:
    0005 32                6             reti
                           7
    0006                   8     _start:
    0006 2403              9             add A, #03h
    0008 7803             10             mov R0, #var
    000A 26               11             add A, @R0
    000B 33               12             rlc A
    000C C0E0             13             push ACC
    000E 120005           14             lcall _stuff
    0011 0113             15             ajmp _end
    0013                  16     _end:
    0013 020013           17             ljmp _end ; while (1) {}
                          18             end
  */

  // if(CheckBox8->State==cbUnchecked)
  {
    for (i = 0; i < 100; i++)
      CODE[i] = 0; //����� ����������� ������

    //CODE                MNEMONICS                                 CORRECTNES
    CODE[0x00] = 0x02;
    CODE[0x01] = 0;
    CODE[0x02] = 0x06; // ljmp _start (ljmp at 0x06)                +

    CODE[0x05] = 0x32; // _stuff: reti                              +

    CODE[0x06] = 0x24; // _start: add A,                            -
    CODE[0x07] = 0x03; //               #03h
    CODE[0x08] = 0x78; //         mov R0,                           -
    CODE[0x09] = 0x03; //               #var (which is at 0x03)
    CODE[0x0A] = 0x26; //         add A, @R0                        -
    CODE[0x0B] = 0x33; //         rlc A                             -
    CODE[0x0C] = 0xC0; //         push                              -
    CODE[0x0D] = 0xE0; //               ACC
    CODE[0x0E] = 0x12; //         lcall                             +
    CODE[0x0F] = 0x00; //               _st
    CODE[0x10] = 0x05; //                  uff (which is at 05)
    CODE[0x11] = 0;    // THIS IS THE END INSTEAD OF WHILE (1) {}   +
  }

  Instr->Clear();
  IE = 0;             //����� �������� ����������
  EX1 = EX0 = EA = 0; //������ ����������
  //����� ����� ���������� �� ������
  CheckBox1->State = cbUnchecked; //�����������
  CheckBox2->State = cbUnchecked; // EX1=0
  CheckBox3->State = cbUnchecked; // EX0=0
  CheckBox4->State = cbUnchecked; // EA=0

  PC = 0; //������ ���������
  Edit7->Text = Ram[1];
  DPTR = DPH << 8 | DPL;
  Edit15->Text = DPTR;
  ComboBox2->Clear(); //����� ���� ���������
  ComboBox6->Clear(); //����� �����
  for (char i = 0; i < 8; i++)
    Reg(i);   //�������� �������� � ��������� !=0
  StateMCU(); //��������� ��������� ���������
  i1 = 1;     //������ ����� � ��������� DCMi
  // CheckBox8->State=cbUnchecked;
  CheckBox9->State = cbUnchecked;
}

//------��������� ���� --------------------------------------

//������� ������� ��������� - ���������� ����-���������
//����� ������, �������� ������ �����
void __fastcall TForm1::Button2Click(TObject *Sender) {
    Edit3->Text = "";
  // 0.0 mk
  GoToInt(); //����� ����������� �������� ���������� - ������� � ����
  MicroCodMem("Unicontr=Eintra, Unicontr=Wrtcon ");
  //�������� ������� � ������� ������� ��� ���������� ������� � ������� �������
  if (!intra) {
    RAMK = 0x05;
    goto decoder;
  } //�� �������� ��� ��� �������
  intra = 0;
  RAMK++;

  // 0.1 mk  ���� � ����������
  SP++;
  RAMK++;
  MicroCodMem("Unireg8=Cntsp,Unireg8=Incsp ");
  // 0.2 mk
  Ram[SP++] = PC;
  RAMK++;
  MicroCodMem("SelbusB=Pcl,SelbusA=Sp,Unireg8=Cntsp,\
                                      Unireg8=Incsp,Unibus8=WramNsfr ");
  // 0.3 mk
  Ram[SP] = PC >> 8;
  PC = vect;
  RAMK++; // ClearInt();
  MicroCodMem("SelbusB=Pch,SelbusA=Sp,Unibus8=WramNsfr,Selpc=Vect");
  // 0.4 mk ���������� ����� � ���������� PC=vect
  Ram[Sp] = SP;
  RAMK = 0;
  Ram[Tcon] = TCON ^ Ram[Tcon];
  P3 ^= TCON;
  MicroCodMem("SelbusB=Sp,SelbusA=Asfr,Adsfr=Sp,Unicontr=Clramk,Unibus8=Wram,\
             Unicontr=Wrtcon ");

  //-------------------------------------
  decoder : // 0.5 mk
  {
    IR = CODE[PC++];
    RAMK = (ADC[IR] << 3) + 1;
    MicroCodMem("SelbusB=Code,Unicontr =Wrir,Unicod16=Incpc ");
  }

  switch (ADC[IR]) //������������� �������
  {
  case 0: // Nop
  {
    Instr->Text = "Nop";
    RAMK = 0;
    goto finish;
  }
  case 1: // ljmp adr
  {
    Wrk = CODE[PC++];
    RAMK++;
    MicroCodMem("SelbusB=Code,Uniralu=Wrwrk,Unicod16=Incpc ");
  }

    {
      // 2.2 ������������ - ������ �������� ����� ������ � PCL
      PC = CODE[PC] | (Wrk << 8);
      RAMK = 0;
      MicroCodMem("Selpc=Call,SelbusB=Code,Unicod16=Wrpc,Unicontr=Ramk1 ");
      itoa(PC, &ss[0], 16); //������������ ���������
      char stroka[10] = "ljmp ";
      Instr->Text = StrCat(stroka, ss);
    }
    goto finish;
  case 2: // inc ri
  {
    char addr = (PSW & 0x18) + (IR & 0x07);
    Ram[addr]++;
    RAMK = 0;

    itoa(IR & 0x07, &ss[0], 16); //������������ ���������
    char stroka[] = "inc r";
    Instr->Text = StrCat(stroka, ss);
    Edit7->Text = Ram[addr];
  }
    goto finish;
  case 3: // inc ad
  {
    // 5.1 ������������
    PB = CODE[PC++];
    RAMK++;

    //������������ � ����� ��������� �������
    itoa(PB, &ss[0], 16); //������������ ���������
    char stroka[10] = "inc ";
    Instr->Text = StrCat(stroka, ss);
    MicroCodMem("SelbusB=Ram,Unibus8=Ari,Uniralu=Wrpb ");
  }
    {
      // 5.2 ������������ - �������� � ��� � ������������ ��������� ����������
      Wrk = Ram[PB];
      RAMK++;
      Ram[PB] = Wrk + 1;
      RAMK++;
      char tt[] = "inc  "; //������������ ���������
      PSWC(&tt[0]);        //������������ ���������
      MicroCodMem("SelbusB=F,Mop=Add,Selpsw=Bitsw,Unibit=Wlocpsw,SelbusA=Asfr,"
                  "Adsfr=Acc,Unibus8=Wram,Selpa=Acc,Selpb=Pb  ");
    }
    // 5.3 ������������ -���������� PsW � SFR
    {
      Ram[Psw] = PSW;
      RAMK = 0;
      Edit7->Text = Ram[PB];
      MicroCodMem(
          "SelbusB=Psw,SelbusA=Asfr,Adsfr=Psw,Unibus8=Wram,Unicontr=Ramk1 ");
    }
    goto finish;
  case 4: // inc dptr
  {
    DPL++;
    Cc = DPL & 0x0800;
    DPL &= 0xff;
    RAMK++;
    DPH += Cc;
    RAMK = 0;
    DPTR = DPH << 8 | DPL;
    Instr->Text = "inc dptr";
    Edit15->Text = DPTR;
  }
    goto finish;
  case 5: // cpl a
  {
    ACC = ~ACC & 0xffff, Ram[Acc] = ACC;
    RAMK = 0;
    Instr->Text = "cpl a";
  }
    goto finish;
  case 6: // xchd a,@ri
  {
    // 11.1 -������ ��������� �� ��� � ������ Code
    PB = Ram[IR & 1]; // CODE[PC++];
    RAMK++;
    MicroCodMem("SelbusB=Code,Uniralu=WrpbUnicod16=Incpc ");
    itoa(PB, &ss[0], 16); //������������ ���������
    char stroka[] = "xchd a,@r";
    Instr->Text = StrCat(stroka, ss);
  }
    // 11.2 - - �������� � ��� � ������������ ��������� ����������
    {
      Wrk = ACC;
      RAMK++;
      ACC = (ACC & 0xfff0) | Ram[PB];
      RAMK++;
      Ram[PB] = (Ram[PB] & 0xfff0) | (Wrk & 0x0f);
      RAMK++;
      Ram[Acc] = ACC;

      PSWC("add");
      MicroCodMem("SelbusB=F,Mop=Add,Selpsw=Bitsw,Unibit=Wlocpsw,SelbusA=Asfr,"
                  "Adsfr=Acc,Unibus8=Wram ");
    }
    // 11.3
    {
      Ram[Psw] = PSW;
      RAMK = 0;
      itoa(Ram[PB], &ss[0], 16);
      Edit7->Text = ss;
      MicroCodMem(
          "SelbusB=Psw,SelbusA=Asfr,Adsfr=Psw,Unibus8=Wram,Unicontr=Ramk1 ");
    }
    goto finish;
  case 7: //  lcall  met // ��������� LCALL ad16
  {

    // 9.1 - ������ ������� ����� ������� � ������������ ���������
    Wrk = CODE[PC++];
    SP++;
    RAMK++;
    MicroCodMem("SelbusB=Code,Uniralu=Wrwrk,Unicod16=Incpc,Uniport=Cntsp,"
                "Uniport=Incsp ");
    itoa((Wrk << 8) + (CODE[PC++]), &ss[0], 16);
    char stroka[10] = "lcall ";
    Instr->Text = StrCat(stroka, ss);
  }
    {
      // 9.2- ������ � ���� PC(7-0) � ������������� ���������
      Ram[SP++] = PC;
      RAMK++;
      MicroCodMem("SelbusB=Pcl,SelbusA=Sp,Unibus8=WramNsfr,Uniport=Cntsp,"
                  "Uniport=Incsp ");
    }

    {
      // 9.3      PC(15-8) --> ����,������������ ������PC
      Ram[SP] = PC >> 8;
      RAMK++;
      PC = (Wrk << 8) + (CODE[--PC]);
      MicroCodMem("SelbusB=Pch,SelbusA=Sp,Unibus8=WramNsfr,Selpc=Acall ");
    }
    {
      // 9.4 ���������� ������������ ��������� � SFR � ���������� ��������������
      Ram[Sp] = SP;
      RAMK = 0;
      MicroCodMem(
          "SelbusB=Sp,SelbusA=Asfr,Adsfr=Sp,Unibus8=Wram,Unicontr=Ramk8 ");
    }
    goto finish;
  case 8: // reti
  {
    // 8.1 ������������
    Wrk = Ram[SP--] << 8;
    RAMK++;
    intra2 = 0; //���������� ���������
    MicroCodMem("SelbusA=Sp,SelbusB=Ram,Uniralu=Wrwrk,Unireg8=Cntsp ");
  }
    // 8.2 ������������
    {
      PC = Wrk | Ram[SP--];
      RAMK++; // PCH.PCL
      MicroCodMem("SelbusA=Sp,SelbusB=Ram,Selpc=Call,Unireg8=Cntsp ");
    }
    // 8.3 ������������
    {
      Ram[Sp] = SP;
      RAMK = 0;
      intra2 = 0;
      MicroCodMem("SelbusB=Sp,Adsfr=Sp,Unibus8=WramNsfr,Unicontr=Ramk1,"
                  "Unicontr=Clrinta ");
      Instr->Text = "reti";
      CheckBox9->State = cbUnchecked;
    }
  //����� ��������� ��������� � HEX-����
  finish: //������� � �������� ���������� RAMK=-
    StateMCU();
    ComboBox2->Clear(); //����� ���� ���������
    for (char i = 0; i < 8; i++)
      Reg(i); //�������� �������� � ��������� !=0
    ComboBox6->Clear(); //����� �����
    for (char i = 8; i < (SP + 5); i++) {
      Stack(i);
    }
    //---------------------------------------
    //���������� ���������� �� Nop - ����������������
    if (CheckBox7->State == cbChecked) {
      for (char i = 8; i < (SP + 5); i++) {
        Stack(i);
      }
      //��������� ����� ����� RET,RETI
      Instr->Text = "����� ���������";
      PC--;
      CheckBox7->State == cbUnchecked;
    }
    break;
  default:
    {
        Edit3->Text = "weirdo";
    }
  } // switch
}

//---------------------------------------------------------------------------

char Chi;
void __fastcall TForm1::CheckBox1Click(TObject *Sender) { //"����������� ���������"
  if (Chi == 0) {
    CheckBox1->State = cbChecked;
    Chi = 1;
  } else {
    CheckBox1->State = cbUnchecked;
    Chi = 0;
  }
}
//---------------------------------------------------------------------------

void Creatwrsfr(void) //������� ������� SFR � wrsfr � �������
{
  WRSFR[Sp & 0x7f] = 2;
  WRSFR[Acc & 0x7f] = 1;
  WRSFR[Dph & 0x7f] = 3;
  WRSFR[Dpl & 0x7f] = 4;
  WRSFR[b & 0x7f] = 3;
  WRSFR[Psw & 0x7f] = 4;
  WRSFR[p3 & 0x7f] = 10;

  WRSFR[p0 & 0x7f] = 7;
  WRSFR[p1 & 0x7f] = 5;
  WRSFR[p2 & 0x7f] = 9;
  WRSFR[Ie & 0x7f] = 13;
  WRSFR[Ip & 0x7f] = 12;
  WRSFR[Tcon & 0x7f] = 3;
}

uchar VECTOR[16];
void Creatvector(void) //������� ����������
{
  for (uchar i = 0; i < 16; i++)
    VECTOR[i] = 0;
  VECTOR[01] = 0x03;
  VECTOR[02] = 0x0B;
  VECTOR[04] = 0x13;
  VECTOR[0x8] = 0x1B;
}

void ToFile(AnsiString S, char *ss) { //�������  .mfi ���� ��� maxplus
  char *std = "     ";
  char *std1 = "     ";
  char *std2 = "     ";
  char *std3 = "     ";
  std2 = std;
  std3 = std1;
  FILE *tmpFILE = fopen(S.c_str(), "w");
  if (tmpFILE != NULL) {
    if (strcmp(ss, "CODE") == 0) {
      fprintf(tmpFILE, " %s \n", "DEPTH = 2048;");
      fprintf(tmpFILE, " %s \n", "WIDTH = 8;");
      goto mms;
    }
    if (strcmp(ss, "Xdata") == 0) {
      fprintf(tmpFILE, " %s \n", "DEPTH = 2048;");
      fprintf(tmpFILE, " %s \n", "WIDTH = 8;");
      goto mms;
    }
    if (strcmp(ss, "ADC") == 0) {
      fprintf(tmpFILE, " %s \n", "DEPTH = 256;");
      fprintf(tmpFILE, " %s \n", "WIDTH = 7;");
      goto mms;
    }
    if (strcmp(ss, "ROMM") == 0) {
      fprintf(tmpFILE, " %s \n", "DEPTH = 128;");
      fprintf(tmpFILE, " %s \n", "WIDTH = 10;");
      goto mms;
    }
    if (strcmp(ss, "WRSFR") == 0) {
      fprintf(tmpFILE, " %s \n", "DEPTH = 128;");
      fprintf(tmpFILE, " %s \n", "WIDTH = 4;");
      goto mms;
    }
    if (strcmp(ss, "VECTOR") == 0) {
      fprintf(tmpFILE, " %s \n", "DEPTH = 16;");
      fprintf(tmpFILE, " %s \n", "WIDTH = 6;");
      goto mms;
    }

    fprintf(tmpFILE, " %s \n", "DEPTH = 64;");
    if (strcmp(ss, "DCM1") == 0)
      fprintf(tmpFILE, " %s \n", "WIDTH = 7;");
    else if (strcmp(ss, "DCM2") == 0)
      fprintf(tmpFILE, " %s \n", "WIDTH = 6;");
    else if (strcmp(ss, "DCM3") == 0)
      fprintf(tmpFILE, " %s \n", "WIDTH = 7;");
    else if (strcmp(ss, "DCM4") == 0)
      fprintf(tmpFILE, " %s \n", "WIDTH = 19;");
    else if (strcmp(ss, "DCM6") == 0)
      fprintf(tmpFILE, " %s \n", "WIDTH = 5;");
    else if (strcmp(ss, "DCM7") == 0)
      fprintf(tmpFILE, " %s \n", "WIDTH = 17;");
    else if (strcmp(ss, "DCM8") == 0)
      fprintf(tmpFILE, " %s \n", "WIDTH = 3;");
  mms:
    fprintf(tmpFILE, " %s \n", "ADDRESS_RADIX = HEX;");
    fprintf(tmpFILE, " %s \n", "DATA_RADIX = HEX;");
    fprintf(tmpFILE, " %s \n", "CONTENT");
    fprintf(tmpFILE, " %s \n", "BEGIN");
  }

  if (strcmp(ss, "ADC") == 0) {
    for (uint i = 0; i < 256; i++)
      fprintf(tmpFILE, " %s %s %s %s  \n", itoa(i, std, 16), "  :  ",
              itoa(ADC[i], std1, 16), " ;");
    goto mms1;
  }
  if (strcmp(ss, "CODE") == 0) {
    for (uint i = 0; i < 2048; i++)
      fprintf(tmpFILE, " %s %s %s %s  \n", itoa(i, std, 16), "  :  ",
              itoa(CODE[i], std1, 16), " ;");
    goto mms1;
  }
  if (strcmp(ss, "VECTOR") == 0) {
    for (uchar i = 0; i < 16; i++)
      fprintf(tmpFILE, " %s %s %s %s  \n", itoa(i, std, 16), "  :  ",
              itoa(VECTOR[i], std1, 16), " ;");
    goto mms1;
  }

  if (strcmp(ss, "ROMM") == 0) {
    for (uint i = 0; i < 128; i++)
      fprintf(tmpFILE, " %s %s %s %s  \n", itoa(i, std, 16), "  :  ",
              itoa(ROMM[i], std1, 16), " ;");
    goto mms1;
  }
  if (strcmp(ss, "WRSFR") == 0) {
    for (uint i = 0; i < 128; i++)
      fprintf(tmpFILE, " %s %s %s %s  \n", itoa(i, std, 16), "  :  ",
              itoa(WRSFR[i], std1, 16), " ;");
    goto mms1;
  }
  for (uchar i = 0; i < 64; i++) {
    if (strcmp(ss, "DCM1") == 0)
      fprintf(tmpFILE, " %s %s %s %s  \n", itoa(i, std, 16), "  :  ",
              itoa(DCM1[i], std1, 16), " ;");
    else if (strcmp(ss, "DCM2") == 0)
      fprintf(tmpFILE, " %s %s %s %s  \n", itoa(i, std, 16), "  :  ",
              itoa(DCM2[i], std1, 16), " ;");
    else if (strcmp(ss, "DCM3") == 0)
      fprintf(tmpFILE, " %s %s %s %s  \n", itoa(i, std, 16), "  :  ",
              itoa(DCM3[i], std1, 16), " ;");
    else if (strcmp(ss, "DCM4") == 0)
      fprintf(tmpFILE, " %s %s %s %s  \n", itoa(i, std, 16), "  :  ",
              itoa(DCM4[i], std1, 16), " ;");

    else if (strcmp(ss, "DCM7") == 0)
      fprintf(tmpFILE, " %s %s %s %s  \n", itoa(i, std, 16), "  :  ",
              itoa(DCM7[i], std1, 16), " ;");
    else if (strcmp(ss, "DCM8") == 0)
      fprintf(tmpFILE, " %s %s %s %s  \n", itoa(i, std, 16), "  :  ",
              itoa(DCM8[i], std1, 16), " ;");

    else
      break;
    std = std2;
    std1 = std3;
  }

mms1:
  fprintf(tmpFILE, " %s \n", "END;");
  fclose(tmpFILE);
}

void __fastcall TForm1::Button7Click(TObject *Sender) { //��������� ��� � .mfi �����

  ToFile("DCM1.mif", "DCM1"); // 16mem
  ToFile("DCM2.mif", "DCM2"); // port
  ToFile("DCM3.mif", "DCM3"); // bit
  ToFile("DCM4.mif", "DCM4"); // ralu
  ToFile("DCM6.mif", "DCM6"); // intrupt
  ToFile("DCM7.mif", "DCM7"); // bus8
  ToFile("DCM8.mif", "DCM8"); // control
  ToFile("ADC.mif", "ADC");   //���-->����� ��
  ToFile("ROMM.mif", "ROMM"); //��� ��
  ToFile("CODE.mif", "CODE"); //���������
  Creatwrsfr();
  ToFile("WR8.mif", "WR8"); //������ SFR
  Creatvector();
  ToFile("VECTOR.mif", "VECTOR"); // vect ����������
  Edit4->Text = "����� �������";
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button3Click(TObject *Sender) { //������ ���������� �� H/L ������INT1 ��� IT1=1
  IE1 = 1;
  TCON = TCON | 0x8;
  Ram[Tcon] = TCON;
  CheckBox6->State = cbChecked;
  P3 ^= 0x8;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::CheckBox2Click(TObject *Sender) { //������� ��������� ����� INT1

  EX1 ^= 1;
  if (EX1) {
    IE = IE | 0x4; //��������� ����� � �������� IE
    CheckBox2->State = cbChecked;
  } else {
    IE = IE & 0xfb; //�������� ����� � ��������
    CheckBox2->State = cbUnchecked;
  }
  Ram[Ie] = IE;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::CheckBox3Click(TObject *Sender) { //������� ��������� ����� INT0

  EX0 ^= 1;
  if (EX0) {
    IE = IE | 0x1; //��������� ����� � �������� IE
    CheckBox3->State = cbChecked;
  } else {
    IE = IE & 0xfe; //�������� ����� � �������� IE
    CheckBox3->State = cbUnchecked;
  }
  Ram[Ie] = IE;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::CheckBox4Click(TObject *Sender) { //������� ���������� ����������

  EA ^= 1;
  if (EA) {
    IE = IE | 0x80; //��������� ��� � �������� IE
    CheckBox4->State = cbChecked;
  } else {
    IE = IE & 0x7f; //�������� ��� � �������� IE
    CheckBox4->State = cbUnchecked;
  }
  Ram[Ie] = IE;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Edit7DblClick(TObject *Sender) { //���������� �� ���������� ���� ��� RAM
  char s;
  char ss[3];
  int L;
  AnsiString Q = Edit7->Text;
  if (Q[1] == '@') {
    s = Q[3];
    Edit7->Text = itoa(Ram[Ram[s & 1]], ss, 16);
  } else
    Edit7->Text = itoa(Ram[StrToInt(Q.c_str())], ss, 16);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Edit6DblClick(TObject *Sender) { //��� ��� SFR
  uint numb = 0xffff;
  AnsiString S = Edit6->Text;
  char *ss;
  ss = S.c_str();
  if (strcmp(ss, "ACC") == 0)
    numb = ACC;
  if (strcmp(ss, "SP") == 0)
    numb = SP;
  if (strcmp(ss, "DPH") == 0)
    numb = DPH;
  if (strcmp(ss, "DPL") == 0)
    numb = DPL;
  if (strcmp(ss, "PSW") == 0)
    numb = PSW;
  if (strcmp(ss, "P3") == 0)
    numb = P3;
  if (strcmp(ss, "P0") == 0)
    numb = P0;
  if (strcmp(ss, "P1") == 0)
    numb = P1;
  if (strcmp(ss, "P2") == 0)
    numb = P2;
  if (strcmp(ss, "TCON") == 0)
    numb = TCON;
  if (strcmp(ss, "IE") == 0)
    numb = IE;
  if (strcmp(ss, "IP") == 0)
    numb = IP;
  if (numb != 0xffff)
    Edit6->Text = itoa(numb, ss, 16);
  else
    Edit6->Text = "������";
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Edit9DblClick(TObject *Sender) { //��������� ���� xdata
  char s;
  char ss[3];
  int L;
  AnsiString Q = Edit9->Text;
  if (Q[1] == '@') {
    s = Q[3];
    Edit9->Text = itoa(Xdata[(P2 << 8) + Ram[s & 1]], ss, 16);
  } else
    Edit9->Text = itoa(Xdata[StrToInt(Q.c_str())], ss, 16);
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

void __fastcall TForm1::CheckBox6Click(TObject *Sender) { //����� ������� IE1
  /* IE0^=1;
    if (~IE0)
       {TCON=TCON|2; CheckBox5->State= cbChecked;
           }
    else
       { TCON=TCON&0xfd;
                     CheckBox5->State= cbUnchecked;
                     }
           Ram[Tcon]=TCON;
        */
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button10Click(TObject *Sender) { //������ ���������� �� H/L ������INT0 ��� IT0=1
  
  IE0 = 1;
  TCON = TCON | 0x2;
  Ram[Tcon] = TCON;
  CheckBox5->State = cbChecked;
  P3 ^= 0x4;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button13Click(TObject *Sender) { //������ ������ ���� Code
  //� �� ���� ��������� ��� ����, � ��������� ����������
  AnsiString S;
  char j = 0;
  char numb = 0;
  int adr = 0;
  int y;
  char x;
  if (OpenDialog1->Execute()) {
    S = OpenDialog1->FileName; // S=���� � �����

    hdisk = CreateFile(S.c_str(), GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
  }
  if (hdisk) //������ ������
  {
    j = 0;
    do {
      numb = 0;
      adr = 0;
      do {
        ReadFile(hdisk, &x, 1, &bufd, 0);
        x = (x < 0x3a) ? x & 0xf : x - 'A' + 10;
        if ((j >= 1) && (j < 3))
          numb = numb * 16 + x;
        if ((j >= 3) && (j < 7)) {
          if (numb == 0)
            goto exit;
          adr = adr * 16 + x;
        }
        if (j >= 9)
          if (j & 1)
            y = x;
          else {
            y = y * 16 + x;
            CODE[adr++] = y;
            numb--;
            if (numb == 0)
              goto brk;
          }
        j++;
      } while (1);
    brk:
      do {
        ReadFile(hdisk, &x, 1, &bufd, 0);
      } while (x != ':');
      j = 1;
    } while (1);
  exit:
    Edit3->Text = "File open";
  } else
    Edit3->Text = "No open";
  CloseHandle(hdisk);
  CheckBox8->State = cbChecked;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Edit2DblClick(TObject *Sender) { //��������� ���� P3
  char i;
  int ss;
  char qq[18];
  AnsiString S;
  AnsiString Q = Edit2->Text;
  ss = atoi(Q.c_str());
  itoa(ss, &qq[0], 2);
  qq[17] = 0;
  for (i = 0; i < 8; i++) {
    qq[16 - i] = qq[7 - i]; // qq[7-i]=' ';
  }
  Edit2->Text = &qq[0];
}
//---------------------------------------------------------------------------
