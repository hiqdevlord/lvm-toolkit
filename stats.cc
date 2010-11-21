//#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

#include "stats.h"

#include "dSFMT-src-2.0/dSFMT.h"

// The random seed
DEFINE_int32(random_seed, 101, "what seed value to use");


// The Mersenne Twister
dsfmt_t dsfmt;

double NEGINF = -0/1;
// Our tolerance for numerical (under)overflow.
double epsilonValue = 1e-4; // really???? this low??? for condor.
// double epsilonValue = 1e-10;

#define BOUNDLO(x) (((x)<epsilonValue)?(epsilonValue):(x))
#define BOUNDHI(x) (((x)>(1-epsilonValue))?(1-epsilonValue):(x))
#define BOUND(x) (BOUNDLO(BOUNDHI(x)))
#define BOUNDPROB(x) (((x)<-300)?(-300):((((x)>300)?(300):(x))))


    /* The digamma function is the derivative of gammaln.

       Reference:
        J Bernardo,
        Psi ( Digamma ) Function,
        Algorithm AS 103,
        Applied Statistics,
        Volume 25, Number 3, pages 315-317, 1976.

        From http://www.psc.edu/~burkardt/src/dirichlet/dirichlet.f
        (with modifications for negative numbers and extra precision)
    */
    double digamma(double x)
    {
      static const double 
        c = 12,
        d1 = -0.57721566490153286,
        d2 = 1.6449340668482264365, /* pi^2/6 */
        s = 1e-6,
        s3 = 1./12,
        s4 = 1./120,
        s5 = 1./252,
        s6 = 1./240,
        s7 = 1./132,
        s8 = 691/32760,
        s9 = 1/12,
        s10 = 3617/8160;
      double result;
#if 0
      static double cache_x = 0;
      static int hits = 0, total = 0;
      total++;
      if(x == cache_x) {
        hits++;
      }
      if(total % 1000 == 1) {
        printf("hits = %d, total = %d, hits/total = %g\n", hits, total, 
           ((double)hits)/total);
      }
      cache_x = x;
#endif
      /* Illegal arguments */
      if((isinf(x) == -1) || isnan(x)) {
        return NAN;
      }
      /* Singularities */
      if((x <= 0) && (floor(x) == x)) {
        return -INFINITY;
      }
      /* Negative values */
      /* Use the reflection formula (Jeffrey 11.1.6):
       * digamma(-x) = digamma(x+1) + pi*cot(pi*x)
       *
       * This is related to the identity
       * digamma(-x) = digamma(x+1) - digamma(z) + digamma(1-z)
       * where z is the fractional part of x
       * For example:
       * digamma(-3.1) = 1/3.1 + 1/2.1 + 1/1.1 + 1/0.1 + digamma(1-0.1)
       *               = digamma(4.1) - digamma(0.1) + digamma(1-0.1)
       * Then we use
       * digamma(1-z) - digamma(z) = pi*cot(pi*z)
       */
      if(x < 0) {
        return digamma(1-x) + 3.14159265/tan(-3.14159265*x);
      }
      /* Use Taylor series if argument <= S */
      if(x <= s) return d1 - 1/x + d2*x;
      /* Reduce to digamma(X + N) where (X + N) >= C */
      result = 0;
      while(x < c) {
        result -= 1/x;
        x++;
      }
      /* Use de Moivre's expansion if argument >= C */
      /* This expansion can be computed in Maple via asympt(Psi(x),x) */
      if(x >= c) {
        double r = 1/x;
        result += log(x) - 0.5*r;
        r *= r;
        result -= r * (s3 - r * (s4 - r * (s5 - r * (s6 - r * s7))));
      }
      return result;
    }

    /* The trigamma function is the derivative of the digamma function.

       Reference:

        B Schneider,
        Trigamma Function,
        Algorithm AS 121,
        Applied Statistics, 
        Volume 27, Number 1, page 97-99, 1978.

        From http://www.psc.edu/~burkardt/src/dirichlet/dirichlet.f
        (with modification for negative arguments and extra precision)
    */
    double trigamma(double x)
    {
      double 
        small = 1e-4,
        large = 8,
        c = 1.6449340668482264365, /* pi^2/6 = Zeta(2) */
        c1 = -2.404113806319188570799476,  /* -2 Zeta(3) */
        b2 =  1./6,
        b4 = -1./30,
        b6 =  1./42,
        b8 = -1./30,
        b10 = 5./66;
      double result;
      /* Illegal arguments */
      if((isinf(x) == -1) || isnan(x)) {
        return NAN;
      }
      /* Singularities */
      if((x <= 0) && (floor(x) == x)) {
        return -INFINITY;
      }
      /* Negative values */
      /* Use the derivative of the digamma reflection formula:
       * -trigamma(-x) = trigamma(x+1) - (pi*csc(pi*x))^2
       */
      if(x < 0) {
        result = M_PI/sin(-M_PI*x);
        return -trigamma(1-x) + result*result;
      }
      /* Use Taylor series if argument <= small */
      if(x <= small) {
        return 1/(x*x) + c + c1*x;
      }
      result = 0;
      /* Reduce to trigamma(x+n) where ( X + N ) >= B */
      while(x < large) {
        result += 1/(x*x);
        x++;
      }
      /* Apply asymptotic formula when X >= B */
      /* This expansion can be computed in Maple via asympt(Psi(1,x),x) */
      if(x >= large) {
        double r = 1/(x*x);
        result += 0.5*r + (1 + r*(b2 + r*(b4 + r*(b6 + r*(b8 + r*b10)))))/x;
      }
      return result;
    }

    double invdigamma(double y) {
      double x = (y <= -2.22) ? (-1 / (y + 0.5772156649015353)) : (exp(y) + 0.5);
      /* never need more than 5 iterations; we unfold them ourselves */
      x = x - (digamma(x) - y) / trigamma(x);
      x = x - (digamma(x) - y) / trigamma(x);
      x = x - (digamma(x) - y) / trigamma(x);
      x = x - (digamma(x) - y) / trigamma(x);
      x = x - (digamma(x) - y) / trigamma(x);
      return x;
    }

#define ABS(x) ((x) >= 0 ? (x) : -(x))
#define MIN(a,b) ((a) <= (b) ? (a) : (b))
#define MAX(a,b) ((a) >= (b) ? (a) : (b))
    double genbet(double aa,double bb)
    /*
    **********************************************************************
         double genbet(double aa,double bb)
                   GeNerate BETa random deviate
                                  Function
         Returns a single random deviate from the beta distribution with
         parameters A and B.  The density of the beta is
                   x^(a-1) * (1-x)^(b-1) / B(a,b) for 0 < x < 1
                                  Arguments
         aa --> First parameter of the beta distribution
           
         bb --> Second parameter of the beta distribution
           
                                  Method
         R. C. H. Cheng
         Generating Beta Variatew with Nonintegral Shape Parameters
         Communications of the ACM, 21:317-322  (1978)
         (Algorithms BB and BC)
    **********************************************************************
    */
    {
#define expmax 89.0
#define infnty 1.0E38
    static double olda = -1.0;
    static double oldb = -1.0;
    static double genbet,a,alpha,b,beta,delta,gamma,k1,k2,r,s,t,u1,u2,v,w,y,z;
    static long qsame;

        qsame = olda == aa && oldb == bb;
        if(qsame) goto S20;
        if(!(aa <= 0.0 || bb <= 0.0)) goto S10;
        fputs(" AA or BB <= 0 in GENBET - Abort!",stderr);
        fprintf(stderr," AA: %16.6E BB %16.6E\n",aa,bb);
        exit(1);
    S10:
        olda = aa;
        oldb = bb;
    S20:
        if(!(MIN(aa,bb) > 1.0)) goto S100;
    /*
         Alborithm BB
         Initialize
    */
        if(qsame) goto S30;
        a = MIN(aa,bb);
        b = MAX(aa,bb);
        alpha = a+b;
        beta = sqrt((alpha-2.0)/(2.0*a*b-alpha));
        gamma = a+1.0/beta;
    S30:
    S40:
        u1 = ranf();
    /*
         Step 1
    */
        u2 = ranf();
        v = beta*log(u1/(1.0-u1));
        if(!(v > expmax)) goto S50;
        w = infnty;
        goto S60;
    S50:
        w = a*exp(v);
    S60:
        z = pow(u1,2.0)*u2;
        r = gamma*v-1.3862944;
        s = a+r-w;
    /*
         Step 2
    */
        if(s+2.609438 >= 5.0*z) goto S70;
    /*
         Step 3
    */
        t = log(z);
        if(s > t) goto S70;
    /*
         Step 4
    */
        if(r+alpha*log(alpha/(b+w)) < t) goto S40;
    S70:
    /*
         Step 5
    */
        if(!(aa == a)) goto S80;
        genbet = w/(b+w);
        goto S90;
    S80:
        genbet = b/(b+w);
    S90:
        goto S230;
    S100:
    /*
         Algorithm BC
         Initialize
    */
        if(qsame) goto S110;
        a = MAX(aa,bb);
        b = MIN(aa,bb);
        alpha = a+b;
        beta = 1.0/b;
        delta = 1.0+a-b;
        k1 = delta*(1.38889E-2+4.16667E-2*b)/(a*beta-0.777778);
        k2 = 0.25+(0.5+0.25/delta)*b;
    S110:
    S120:
        u1 = ranf();
    /*
         Step 1
    */
        u2 = ranf();
        if(u1 >= 0.5) goto S130;
    /*
         Step 2
    */
        y = u1*u2;
        z = u1*y;
        if(0.25*u2+z-y >= k1) goto S120;
        goto S170;
    S130:
    /*
         Step 3
    */
        z = pow(u1,2.0)*u2;
        if(!(z <= 0.25)) goto S160;
        v = beta*log(u1/(1.0-u1));
        if(!(v > expmax)) goto S140;
        w = infnty;
        goto S150;
    S140:
        w = a*exp(v);
    S150:
        goto S200;
    S160:
        if(z >= k2) goto S120;
    S170:
    /*
         Step 4
         Step 5
    */
        v = beta*log(u1/(1.0-u1));
        if(!(v > expmax)) goto S180;
        w = infnty;
        goto S190;
    S180:
        w = a*exp(v);
    S190:
        if(alpha*(log(alpha/(b+w))+v)-1.3862944 < log(z)) goto S120;
    S200:
    /*
         Step 6
    */
        if(!(a == aa)) goto S210;
        genbet = w/(b+w);
        goto S220;
    S210:
        genbet = b/(b+w);
    S230:
    S220:
        return genbet;
#undef expmax
#undef infnty
    }
    double genchi(double df)
    /*
    **********************************************************************
         double genchi(double df)
                    Generate random value of CHIsquare variable
                                  Function
         Generates random deviate from the distribution of a chisquare
         with DF degrees of freedom random variable.
                                  Arguments
         df --> Degrees of freedom of the chisquare
                (Must be positive)
           
                                  Method
         Uses relation between chisquare and gamma.
    **********************************************************************
    */
    {
    static double genchi;

        if(!(df <= 0.0)) goto S10;
        fputs("DF <= 0 in GENCHI - ABORT",stderr);
        fprintf(stderr,"Value of DF: %16.6E\n",df);
        exit(1);
    S10:
        genchi = 2.0*gengam(1.0,df/2.0);
        return genchi;
    }
    double genexp(double av)
    /*
    **********************************************************************
         double genexp(double av)
                        GENerate EXPonential random deviate
                                  Function
         Generates a single random deviate from an exponential
         distribution with mean AV.
                                  Arguments
         av --> The mean of the exponential distribution from which
                a random deviate is to be generated.
                                  Method
         Renames SEXPO from TOMS as slightly modified by BWB to use RANF
         instead of SUNIF.
         For details see:
                   Ahrens, J.H. and Dieter, U.
                   Computer Methods for Sampling From the
                   Exponential and Normal Distributions.
                   Comm. ACM, 15,10 (Oct. 1972), 873 - 882.
    **********************************************************************
    */
    {
    static double genexp;

        genexp = sexpo()*av;
        return genexp;
    }
    double genf(double dfn,double dfd)
    /*
    **********************************************************************
         double genf(double dfn,double dfd)
                    GENerate random deviate from the F distribution
                                  Function
         Generates a random deviate from the F (variance ratio)
         distribution with DFN degrees of freedom in the numerator
         and DFD degrees of freedom in the denominator.
                                  Arguments
         dfn --> Numerator degrees of freedom
                 (Must be positive)
         dfd --> Denominator degrees of freedom
                 (Must be positive)
                                  Method
         Directly generates ratio of chisquare variates
    **********************************************************************
    */
    {
    static double genf,xden,xnum;

        if(!(dfn <= 0.0 || dfd <= 0.0)) goto S10;
        fputs("Degrees of freedom nonpositive in GENF - abort!",stderr);
        fprintf(stderr,"DFN value: %16.6EDFD value: %16.6E\n",dfn,dfd);
        exit(1);
    S10:
        xnum = genchi(dfn)/dfn;
    /*
          GENF = ( GENCHI( DFN ) / DFN ) / ( GENCHI( DFD ) / DFD )
    */
        xden = genchi(dfd)/dfd;
        if(!(xden <= 9.999999999998E-39*xnum)) goto S20;
        fputs(" GENF - generated numbers would cause overflow",stderr);
        fprintf(stderr," Numerator %16.6E Denominator %16.6E\n",xnum,xden);
        fputs(" GENF returning 1.0E38",stderr);
        genf = 1.0E38;
        goto S30;
    S20:
        genf = xnum/xden;
    S30:
        return genf;
    }
    double gengam(double a,double r)
    /*
    **********************************************************************
         double gengam(double a,double r)
               GENerates random deviates from GAMma distribution
                                  Function
         Generates random deviates from the gamma distribution whose
         density is
              (A**R)/Gamma(R) * X**(R-1) * Exp(-A*X)
                                  Arguments
         a --> Location parameter of Gamma distribution
         r --> Shape parameter of Gamma distribution
                                  Method
         Renames SGAMMA from TOMS as slightly modified by BWB to use RANF
         instead of SUNIF.
         For details see:
                   (Case R >= 1.0)
                   Ahrens, J.H. and Dieter, U.
                   Generating Gamma Variates by a
                   Modified Rejection Technique.
                   Comm. ACM, 25,1 (Jan. 1982), 47 - 54.
         Algorithm GD
                   (Case 0.0 <= R <= 1.0)
                   Ahrens, J.H. and Dieter, U.
                   Computer Methods for Sampling from Gamma,
                   Beta, Poisson and Binomial Distributions.
                   Computing, 12 (1974), 223-246/
         Adapted algorithm GS.
    **********************************************************************
    */
    {
    static double gengam;

        gengam = sgamma(r);
        gengam /= a;
        return gengam;
    }
    void genmn(double *parm,double *x,double *work)
    /*
    **********************************************************************
         void genmn(double *parm,double *x,double *work)
                  GENerate Multivariate Normal random deviate
                                  Arguments
         parm --> Parameters needed to generate multivariate normal
                   deviates (MEANV and Cholesky decomposition of
                   COVM). Set by a previous call to SETGMN.
                   1 : 1                - size of deviate, P
                   2 : P + 1            - mean vector
                   P+2 : P*(P+3)/2 + 1  - upper half of cholesky
                                           decomposition of cov matrix
         x    <-- Vector deviate generated.
         work <--> Scratch array
                                  Method
         1) Generate P independent standard normal deviates - Ei ~ N(0,1)
         2) Using Cholesky decomposition find A s.t. trans(A)*A = COVM
         3) trans(A)E + MEANV ~ N(MEANV,COVM)
    **********************************************************************
    */
    {
    static long i,icount,j,p,D1,D2,D3,D4;
    static double ae;

        p = (long) (*parm);
    /*
         Generate P independent normal deviates - WORK ~ N(0,1)
    */
        for(i=1; i<=p; i++) *(work+i-1) = snorm();
        for(i=1,D3=1,D4=(p-i+D3)/D3; D4>0; D4--,i+=D3) {
    /*
         PARM (P+2 : P*(P+3)/2 + 1) contains A, the Cholesky
          decomposition of the desired covariance matrix.
              trans(A)(1,1) = PARM(P+2)
              trans(A)(2,1) = PARM(P+3)
              trans(A)(2,2) = PARM(P+2+P)
              trans(A)(3,1) = PARM(P+4)
              trans(A)(3,2) = PARM(P+3+P)
              trans(A)(3,3) = PARM(P+2-1+2P)  ...
         trans(A)*WORK + MEANV ~ N(MEANV,COVM)
    */
            icount = 0;
            ae = 0.0;
            for(j=1,D1=1,D2=(i-j+D1)/D1; D2>0; D2--,j+=D1) {
                icount += (j-1);
                ae += (*(parm+i+(j-1)*p-icount+p)**(work+j-1));
            }
            *(x+i-1) = ae+*(parm+i);
        }
    }
    void genmul(long n,double *p,long ncat,long *ix)
    /*
    **********************************************************************
     
         void genmul(int n,double *p,int ncat,int *ix)
         GENerate an observation from the MULtinomial distribution
                                  Arguments
         N --> Number of events that will be classified into one of
               the categories 1..NCAT
         P --> Vector of probabilities.  P(i) is the probability that
               an event will be classified into category i.  Thus, P(i)
               must be [0,1]. Only the first NCAT-1 P(i) must be defined
               since P(NCAT) is 1.0 minus the sum of the first
               NCAT-1 P(i).
         NCAT --> Number of categories.  Length of P and IX.
         IX <-- Observation from multinomial distribution.  All IX(i)
                will be nonnegative and their sum will be N.
                                  Method
         Algorithm from page 559 of
     
         Devroye, Luc
     
         Non-Uniform Random Variate Generation.  Springer-Verlag,
         New York, 1986.
     
    **********************************************************************
    */
    {
    static double prob,ptot,sum;
    static long i,icat,ntot;
        CHECK_GE(n, 0) << "N < 0 in GENMUL";
        CHECK_GT(ncat, 1) << "NCAT <= 1 in GENMUL";
        ptot = 0.0F;
        for(i=0; i<ncat-1; i++) {
            CHECK_GE(*(p+i), 0.0F) << "Some P(i) < 0 in GENMUL";
            CHECK_LT(*(p+i), 1.0F) << "Some P(i) > 1 in GENMUL";
            ptot += *(p+i);
        }
        CHECK_LE(ptot, 0.99999F) << "Sum of P(i) > 1 in GENMUL";
    /*
         Initialize variables
    */
        ntot = n;
        sum = 1.0F;
        for(i=0; i<ncat; i++) ix[i] = 0;
    /*
         Generate the observation
    */
        for(icat=0; icat<ncat-1; icat++) {
            prob = *(p+icat)/sum;
            *(ix+icat) = ignbin(ntot,prob);
            ntot -= *(ix+icat);
        if(ntot <= 0) return;
            sum -= *(p+icat);
        }
        *(ix+ncat-1) = ntot;
    /*
         Finished
    */
        return;
    }
    double gennch(double df,double xnonc)
    /*
    **********************************************************************
         double gennch(double df,double xnonc)
               Generate random value of Noncentral CHIsquare variable
                                  Function
         Generates random deviate  from the  distribution  of a  noncentral
         chisquare with DF degrees  of freedom and noncentrality  parameter
         xnonc.
                                  Arguments
         df --> Degrees of freedom of the chisquare
                (Must be > 1.0)
         xnonc --> Noncentrality parameter of the chisquare
                   (Must be >= 0.0)
                                  Method
         Uses fact that  noncentral chisquare  is  the  sum of a  chisquare
         deviate with DF-1  degrees of freedom plus the  square of a normal
         deviate with mean XNONC and standard deviation 1.
    **********************************************************************
    */
    {
    static double gennch;

        if(!(df <= 1.0 || xnonc < 0.0)) goto S10;
        fputs("DF <= 1 or XNONC < 0 in GENNCH - ABORT",stderr);
        fprintf(stderr,"Value of DF: %16.6E Value of XNONC%16.6E\n",df,xnonc);
        exit(1);
    S10:
        gennch = genchi(df-1.0)+pow(gennor(sqrt(xnonc),1.0),2.0);
        return gennch;
    }
    double gennf(double dfn,double dfd,double xnonc)
    /*
    **********************************************************************
         double gennf(double dfn,double dfd,double xnonc)
               GENerate random deviate from the Noncentral F distribution
                                  Function
         Generates a random deviate from the  noncentral F (variance ratio)
         distribution with DFN degrees of freedom in the numerator, and DFD
     degrees of freedom in the denominator, and noncentrality parameter
     XNONC.
                              Arguments
     dfn --> Numerator degrees of freedom
             (Must be >= 1.0)
     dfd --> Denominator degrees of freedom
             (Must be positive)
     xnonc --> Noncentrality parameter
               (Must be nonnegative)
                              Method
     Directly generates ratio of noncentral numerator chisquare variate
     to central denominator chisquare variate.
**********************************************************************
*/
{
static double gennf,xden,xnum;
static long qcond;

    qcond = dfn <= 1.0 || dfd <= 0.0 || xnonc < 0.0;
    if(!qcond) goto S10;
    fputs("In GENNF - Either (1) Numerator DF <= 1.0 or",stderr);
    fputs("(2) Denominator DF < 0.0 or ",stderr);
    fputs("(3) Noncentrality parameter < 0.0",stderr);
    fprintf(stderr,
      "DFN value: %16.6EDFD value: %16.6EXNONC value: \n%16.6E\n",dfn,dfd,
      xnonc);
    exit(1);
S10:
    xnum = gennch(dfn,xnonc)/dfn;
/*
      GENNF = ( GENNCH( DFN, XNONC ) / DFN ) / ( GENCHI( DFD ) / DFD )
*/
    xden = genchi(dfd)/dfd;
    if(!(xden <= 9.999999999998E-39*xnum)) goto S20;
    fputs(" GENNF - generated numbers would cause overflow",stderr);
    fprintf(stderr," Numerator %16.6E Denominator %16.6E\n",xnum,xden);
    fputs(" GENNF returning 1.0E38",stderr);
    gennf = 1.0E38;
    goto S30;
S20:
    gennf = xnum/xden;
S30:
    return gennf;
}
double gennor(double av,double sd)
/*
**********************************************************************
     double gennor(double av,double sd)
         GENerate random deviate from a NORmal distribution
                              Function
     Generates a single random deviate from a normal distribution
     with mean, AV, and standard deviation, SD.
                              Arguments
     av --> Mean of the normal distribution.
     sd --> Standard deviation of the normal distribution.
                              Method
     Renames SNORM from TOMS as slightly modified by BWB to use RANF
     instead of SUNIF.
     For details see:
               Ahrens, J.H. and Dieter, U.
               Extensions of Forsythe's Method for Random
               Sampling from the Normal Distribution.
               Math. Comput., 27,124 (Oct. 1973), 927 - 937.
**********************************************************************
*/
{
static double gennor;

    gennor = sd*snorm()+av;
    return gennor;
}
double genunf(double low,double high)
/*
**********************************************************************
     double genunf(double low,double high)
               GeNerate Uniform Real between LOW and HIGH
                              Function
     Generates a real uniformly distributed between LOW and HIGH.
                              Arguments
     low --> Low bound (exclusive) on real value to be generated
     high --> High bound (exclusive) on real value to be generated
**********************************************************************
*/
{
static double genunf;

    if(!(low > high)) goto S10;
    fprintf(stderr,"LOW > HIGH in GENUNF: LOW %16.6E HIGH: %16.6E\n",low,high);
    fputs("Abort",stderr);
    exit(1);
S10:
    genunf = low+(high-low)*ranf();
    return genunf;
}
void gscgn(long getset,long *g)
/*
**********************************************************************
     void gscgn(long getset,long *g)
                         Get/Set GeNerator
     Gets or returns in G the number of the current generator
                              Arguments
     getset --> 0 Get
                1 Set
     g <-- Number of the current random number generator (1..32)
**********************************************************************
*/
{
#define numg 32L
static long curntg = 1;
    if(getset == 0) *g = curntg;
    else  {
        if(*g < 0 || *g > numg) {
            fputs(" Generator number out of range in GSCGN",stderr);
            exit(0);
        }
        curntg = *g;
    }
#undef numg
}
void gsrgs(long getset,long *qvalue)
/*
**********************************************************************
     void gsrgs(long getset,long *qvalue)
               Get/Set Random Generators Set
     Gets or sets whether random generators set (initialized).
     Initially (data statement) state is not set
     If getset is 1 state is set to qvalue
     If getset is 0 state returned in qvalue
**********************************************************************
*/
{
static long qinit = 0;

    if(getset == 0) *qvalue = qinit;
    else qinit = *qvalue;
}
void gssst(long getset,long *qset)
/*
**********************************************************************
     void gssst(long getset,long *qset)
          Get or Set whether Seed is Set
     Initialize to Seed not Set
     If getset is 1 sets state to Seed Set
     If getset is 0 returns T in qset if Seed Set
     Else returns F in qset
**********************************************************************
*/
{
static long qstate = 0;
    if(getset != 0) qstate = 1;
    else  *qset = qstate;
}
long ignbin(long n,double pp)
/*
**********************************************************************
     long ignbin(long n,double pp)
                    GENerate BINomial random deviate
                              Function
     Generates a single random deviate from a binomial
     distribution whose number of trials is N and whose
     probability of an event in each trial is P.
                              Arguments
     n  --> The number of trials in the binomial distribution
            from which a random deviate is to be generated.
     p  --> The probability of an event in each trial of the
            binomial distribution from which a random deviate
            is to be generated.
     ignbin <-- A random deviate yielding the number of events
                from N independent trials, each of which has
                a probability of event P.
                              Method
     This is algorithm BTPE from:
         Kachitvichyanukul, V. and Schmeiser, B. W.
         Binomial Random Variate Generation.
         Communications of the ACM, 31, 2
         (February, 1988) 216.
**********************************************************************
     SUBROUTINE BTPEC(N,PP,ISEED,JX)
     BINOMIAL RANDOM VARIATE GENERATOR
     MEAN .LT. 30 -- INVERSE CDF
       MEAN .GE. 30 -- ALGORITHM BTPE:  ACCEPTANCE-REJECTION VIA
       FOUR REGION COMPOSITION.  THE FOUR REGIONS ARE A TRIANGLE
       (SYMMETRIC IN THE CENTER), A PAIR OF PARALLELOGRAMS (ABOVE
       THE TRIANGLE), AND EXPONENTIAL LEFT AND RIGHT TAILS.
     BTPE REFERS TO BINOMIAL-TRIANGLE-PARALLELOGRAM-EXPONENTIAL.
     BTPEC REFERS TO BTPE AND "COMBINED."  THUS BTPE IS THE
       RESEARCH AND BTPEC IS THE IMPLEMENTATION OF A COMPLETE
       USABLE ALGORITHM.
     REFERENCE:  VORATAS KACHITVICHYANUKUL AND BRUCE SCHMEISER,
       "BINOMIAL RANDOM VARIATE GENERATION,"
       COMMUNICATIONS OF THE ACM, FORTHCOMING
     WRITTEN:  SEPTEMBER 1980.
       LAST REVISED:  MAY 1985, JULY 1987
     REQUIRED SUBPROGRAM:  RAND() -- A UNIFORM (0,1) RANDOM NUMBER
                           GENERATOR
     ARGUMENTS
       N : NUMBER OF BERNOULLI TRIALS            (INPUT)
       PP : PROBABILITY OF SUCCESS IN EACH TRIAL (INPUT)
       ISEED:  RANDOM NUMBER SEED                (INPUT AND OUTPUT)
       JX:  RANDOMLY GENERATED OBSERVATION       (OUTPUT)
     VARIABLES
       PSAVE: VALUE OF PP FROM THE LAST CALL TO BTPEC
       NSAVE: VALUE OF N FROM THE LAST CALL TO BTPEC
       XNP:  VALUE OF THE MEAN FROM THE LAST CALL TO BTPEC
       P: PROBABILITY USED IN THE GENERATION PHASE OF BTPEC
       FFM: TEMPORARY VARIABLE EQUAL TO XNP + P
       M:  INTEGER VALUE OF THE CURRENT MODE
       FM:  DOUBLEING POINT VALUE OF THE CURRENT MODE
       XNPQ: TEMPORARY VARIABLE USED IN SETUP AND SQUEEZING STEPS
       P1:  AREA OF THE TRIANGLE
       C:  HEIGHT OF THE PARALLELOGRAMS
       XM:  CENTER OF THE TRIANGLE
       XL:  LEFT END OF THE TRIANGLE
       XR:  RIGHT END OF THE TRIANGLE
       AL:  TEMPORARY VARIABLE
       XLL:  RATE FOR THE LEFT EXPONENTIAL TAIL
       XLR:  RATE FOR THE RIGHT EXPONENTIAL TAIL
       P2:  AREA OF THE PARALLELOGRAMS
       P3:  AREA OF THE LEFT EXPONENTIAL TAIL
       P4:  AREA OF THE RIGHT EXPONENTIAL TAIL
       U:  A U(0,P4) RANDOM VARIATE USED FIRST TO SELECT ONE OF THE
           FOUR REGIONS AND THEN CONDITIONALLY TO GENERATE A VALUE
           FROM THE REGION
       V:  A U(0,1) RANDOM NUMBER USED TO GENERATE THE RANDOM VALUE
           (REGION 1) OR TRANSFORMED INTO THE VARIATE TO ACCEPT OR
           REJECT THE CANDIDATE VALUE
       IX:  INTEGER CANDIDATE VALUE
       X:  PRELIMINARY CONTINUOUS CANDIDATE VALUE IN REGION 2 LOGIC
           AND A DOUBLEING POINT IX IN THE ACCEPT/REJECT LOGIC
       K:  ABSOLUTE VALUE OF (IX-M)
       F:  THE HEIGHT OF THE SCALED DENSITY FUNCTION USED IN THE
           ACCEPT/REJECT DECISION WHEN BOTH M AND IX ARE SMALL
           ALSO USED IN THE INVERSE TRANSFORMATION
       R: THE RATIO P/Q
       G: CONSTANT USED IN CALCULATION OF PROBABILITY
       MP:  MODE PLUS ONE, THE LOWER INDEX FOR EXPLICIT CALCULATION
            OF F WHEN IX IS GREATER THAN M
       IX1:  CANDIDATE VALUE PLUS ONE, THE LOWER INDEX FOR EXPLICIT
             CALCULATION OF F WHEN IX IS LESS THAN M
       I:  INDEX FOR EXPLICIT CALCULATION OF F FOR BTPE
       AMAXP: MAXIMUM ERROR OF THE LOGARITHM OF NORMAL BOUND
       YNORM: LOGARITHM OF NORMAL BOUND
       ALV:  NATURAL LOGARITHM OF THE ACCEPT/REJECT VARIATE V
       X1,F1,Z,W,Z2,X2,F2, AND W2 ARE TEMPORARY VARIABLES TO BE
       USED IN THE FINAL ACCEPT/REJECT TEST
       QN: PROBABILITY OF NO SUCCESS IN N TRIALS
     REMARK
       IX AND JX COULD LOGICALLY BE THE SAME VARIABLE, WHICH WOULD
       SAVE A MEMORY POSITION AND A LINE OF CODE.  HOWEVER, SOME
       COMPILERS (E.G.,CDC MNF) OPTIMIZE BETTER WHEN THE ARGUMENTS
       ARE NOT INVOLVED.
     ISEED NEEDS TO BE DOUBLE PRECISION IF THE IMSL ROUTINE
     GGUBFS IS USED TO GENERATE UNIFORM RANDOM NUMBER, OTHERWISE
     TYPE OF ISEED SHOULD BE DICTATED BY THE UNIFORM GENERATOR
**********************************************************************
*****DETERMINE APPROPRIATE ALGORITHM AND WHETHER SETUP IS NECESSARY
*/
{
static double psave = -1.0;
static long nsave = -1;
static long ignbin,i,ix,ix1,k,m,mp,T1;
static double al,alv,amaxp,c,f,f1,f2,ffm,fm,g,p,p1,p2,p3,p4,q,qn,r,u,v,w,w2,x,x1,
    x2,xl,xll,xlr,xm,xnp,xnpq,xr,ynorm,z,z2;

    if(pp != psave) goto S10;
    if(n != nsave) goto S20;
    if(xnp < 30.0) goto S150;
    goto S30;
S10:
/*
*****SETUP, PERFORM ONLY WHEN PARAMETERS CHANGE
*/
    psave = pp;
    p = MIN(psave,1.0-psave);
    q = 1.0-p;
S20:
    xnp = n*p;
    nsave = n;
    if(xnp < 30.0) goto S140;
    ffm = xnp+p;
    m = ffm;
    fm = m;
    xnpq = xnp*q;
    p1 = (long) (2.195*sqrt(xnpq)-4.6*q)+0.5;
    xm = fm+0.5;
    xl = xm-p1;
    xr = xm+p1;
    c = 0.134+20.5/(15.3+fm);
    al = (ffm-xl)/(ffm-xl*p);
    xll = al*(1.0+0.5*al);
    al = (xr-ffm)/(xr*q);
    xlr = al*(1.0+0.5*al);
    p2 = p1*(1.0+c+c);
    p3 = p2+c/xll;
    p4 = p3+c/xlr;
S30:
/*
*****GENERATE VARIATE
*/
    u = ranf()*p4;
    v = ranf();
/*
     TRIANGULAR REGION
*/
    if(u > p1) goto S40;
    ix = xm-p1*v+u;
    goto S170;
S40:
/*
     PARALLELOGRAM REGION
*/
    if(u > p2) goto S50;
    x = xl+(u-p1)/c;
    v = v*c+1.0-ABS(xm-x)/p1;
    if(v > 1.0 || v <= 0.0) goto S30;
    ix = x;
    goto S70;
S50:
/*
     LEFT TAIL
*/
    if(u > p3) goto S60;
    ix = xl+log(v)/xll;
    if(ix < 0) goto S30;
    v *= ((u-p2)*xll);
    goto S70;
S60:
/*
     RIGHT TAIL
*/
    ix = xr-log(v)/xlr;
    if(ix > n) goto S30;
    v *= ((u-p3)*xlr);
S70:
/*
*****DETERMINE APPROPRIATE WAY TO PERFORM ACCEPT/REJECT TEST
*/
    k = ABS(ix-m);
    if(k > 20 && k < xnpq/2-1) goto S130;
/*
     EXPLICIT EVALUATION
*/
    f = 1.0;
    r = p/q;
    g = (n+1)*r;
    T1 = m-ix;
    if(T1 < 0) goto S80;
    else if(T1 == 0) goto S120;
    else  goto S100;
S80:
    mp = m+1;
    for(i=mp; i<=ix; i++) f *= (g/i-r);
    goto S120;
S100:
    ix1 = ix+1;
    for(i=ix1; i<=m; i++) f /= (g/i-r);
S120:
    if(v <= f) goto S170;
    goto S30;
S130:
/*
     SQUEEZING USING UPPER AND LOWER BOUNDS ON ALOG(F(X))
*/
    amaxp = k/xnpq*((k*(k/3.0+0.625)+0.1666666666666)/xnpq+0.5);
    ynorm = -(k*k/(2.0*xnpq));
    alv = log(v);
    if(alv < ynorm-amaxp) goto S170;
    if(alv > ynorm+amaxp) goto S30;
/*
     STIRLING'S FORMULA TO MACHINE ACCURACY FOR
     THE FINAL ACCEPTANCE/REJECTION TEST
*/
    x1 = ix+1.0;
    f1 = fm+1.0;
    z = n+1.0-fm;
    w = n-ix+1.0;
    z2 = z*z;
    x2 = x1*x1;
    f2 = f1*f1;
    w2 = w*w;
    if(alv <= xm*log(f1/x1)+(n-m+0.5)*log(z/w)+(ix-m)*log(w*p/(x1*q))+(13860.0-
      (462.0-(132.0-(99.0-140.0/f2)/f2)/f2)/f2)/f1/166320.0+(13860.0-(462.0-
      (132.0-(99.0-140.0/z2)/z2)/z2)/z2)/z/166320.0+(13860.0-(462.0-(132.0-
      (99.0-140.0/x2)/x2)/x2)/x2)/x1/166320.0+(13860.0-(462.0-(132.0-(99.0
      -140.0/w2)/w2)/w2)/w2)/w/166320.0) goto S170;
    goto S30;
S140:
/*
     INVERSE CDF LOGIC FOR MEAN LESS THAN 30
*/
    qn = pow(q,(double)n);
    r = p/q;
    g = r*(n+1);
S150:
    ix = 0;
    f = qn;
    u = ranf();
S160:
    if(u < f) goto S170;
    if(ix > 110) goto S150;
    u -= f;
    ix += 1;
    f *= (g/ix-r);
    goto S160;
S170:
    if(psave > 0.5) ix = n-ix;
    ignbin = ix;
    return ignbin;
}
long ignnbn(long n,double p)
/*
**********************************************************************
 
     long ignnbn(long n,double p)
                GENerate Negative BiNomial random deviate
                              Function
     Generates a single random deviate from a negative binomial
     distribution.
                              Arguments
     N  --> The number of trials in the negative binomial distribution
            from which a random deviate is to be generated.
     P  --> The probability of an event.
                              Method
     Algorithm from page 480 of
 
     Devroye, Luc
 
     Non-Uniform Random Variate Generation.  Springer-Verlag,
     New York, 1986.
**********************************************************************
*/
{
static long ignnbn;
static double y,a,r;
/*
     ..
     .. Executable Statements ..
*/
/*
     Check Arguments
*/
    CHECK_GE(n, 0) << "N < 0 in IGNNBN";
    CHECK_GT(p, 0.0F) << "P <= 0 in IGNNBN";
    CHECK_LT(p, 1.0F) << "P >= 1 in IGNNBN";
/*
     Generate Y, a random gamma (n,(1-p)/p) variable
*/
    r = (double)n;
    a = p/(1.0F-p);
    y = gengam(a,r);
/*
     Generate a random Poisson(y) variable
*/
    ignnbn = ignpoi(y);
    return ignnbn;
}
long ignpoi(double mu)
/*
**********************************************************************
     long ignpoi(double mu)
                    GENerate POIsson random deviate
                              Function
     Generates a single random deviate from a Poisson
     distribution with mean AV.
                              Arguments
     av --> The mean of the Poisson distribution from which
            a random deviate is to be generated.
     genexp <-- The random deviate.
                              Method
     Renames KPOIS from TOMS as slightly modified by BWB to use RANF
     instead of SUNIF.
     For details see:
               Ahrens, J.H. and Dieter, U.
               Computer Generation of Poisson Deviates
               From Modified Normal Distributions.
               ACM Trans. Math. Software, 8, 2
               (June 1982),163-179
**********************************************************************
**********************************************************************
                                                                      
                                                                      
     P O I S S O N  DISTRIBUTION                                      
                                                                      
                                                                      
**********************************************************************
**********************************************************************
                                                                      
     FOR DETAILS SEE:                                                 
                                                                      
               AHRENS, J.H. AND DIETER, U.                            
               COMPUTER GENERATION OF POISSON DEVIATES                
               FROM MODIFIED NORMAL DISTRIBUTIONS.                    
               ACM TRANS. MATH. SOFTWARE, 8,2 (JUNE 1982), 163 - 179. 
                                                                      
     (SLIGHTLY MODIFIED VERSION OF THE PROGRAM IN THE ABOVE ARTICLE)  
                                                                      
**********************************************************************
      INTEGER FUNCTION IGNPOI(IR,MU)
     INPUT:  IR=CURRENT STATE OF BASIC RANDOM NUMBER GENERATOR
             MU=MEAN MU OF THE POISSON DISTRIBUTION
     OUTPUT: IGNPOI=SAMPLE FROM THE POISSON-(MU)-DISTRIBUTION
     MUPREV=PREVIOUS MU, MUOLD=MU AT LAST EXECUTION OF STEP P OR B.
     TABLES: COEFFICIENTS A0-A7 FOR STEP F. FACTORIALS FACT
     COEFFICIENTS A(K) - FOR PX = FK*V*V*SUM(A(K)*V**K)-DEL
     SEPARATION OF CASES A AND B
*/
{
extern double fsign( double num, double sign );
static double a0 = -0.5;
static double a1 = 0.3333333;
static double a2 = -0.2500068;
static double a3 = 0.2000118;
static double a4 = -0.1661269;
static double a5 = 0.1421878;
static double a6 = -0.1384794;
static double a7 = 0.125006;
static double muold = 0.0;
static double muprev = 0.0;
static double fact[10] = {
    1.0,1.0,2.0,6.0,24.0,120.0,720.0,5040.0,40320.0,362880.0
};
static long ignpoi,j,k,kflag,l,m;
static double b1,b2,c,c0,c1,c2,c3,d,del,difmuk,e,fk,fx,fy,g,omega,p,p0,px,py,q,s,
    t,u,v,x,xx,pp[35];

    if(mu == muprev) goto S10;
    if(mu < 10.0) goto S120;
/*
     C A S E  A. (RECALCULATION OF S,D,L IF MU HAS CHANGED)
*/
    muprev = mu;
    s = sqrt(mu);
    d = 6.0*mu*mu;
/*
             THE POISSON PROBABILITIES PK EXCEED THE DISCRETE NORMAL
             PROBABILITIES FK WHENEVER K >= M(MU). L=IFIX(MU-1.1484)
             IS AN UPPER BOUND TO M(MU) FOR ALL MU >= 10 .
*/
    l = (long) (mu-1.1484);
S10:
/*
     STEP N. NORMAL SAMPLE - SNORM(IR) FOR STANDARD NORMAL DEVIATE
*/
    g = mu+s*snorm();
    if(g < 0.0) goto S20;
    ignpoi = (long) (g);
/*
     STEP I. IMMEDIATE ACCEPTANCE IF IGNPOI IS LARGE ENOUGH
*/
    if(ignpoi >= l) return ignpoi;
/*
     STEP S. SQUEEZE ACCEPTANCE - SUNIF(IR) FOR (0,1)-SAMPLE U
*/
    fk = (double)ignpoi;
    difmuk = mu-fk;
    u = ranf();
    if(d*u >= difmuk*difmuk*difmuk) return ignpoi;
S20:
/*
     STEP P. PREPARATIONS FOR STEPS Q AND H.
             (RECALCULATIONS OF PARAMETERS IF NECESSARY)
             .3989423=(2*PI)**(-.5)  .416667E-1=1./24.  .1428571=1./7.
             THE QUANTITIES B1, B2, C3, C2, C1, C0 ARE FOR THE HERMITE
             APPROXIMATIONS TO THE DISCRETE NORMAL PROBABILITIES FK.
             C=.1069/MU GUARANTEES MAJORIZATION BY THE 'HAT'-FUNCTION.
*/
    if(mu == muold) goto S30;
    muold = mu;
    omega = 0.3989423/s;
    b1 = 4.166667E-2/mu;
    b2 = 0.3*b1*b1;
    c3 = 0.1428571*b1*b2;
    c2 = b2-15.0*c3;
    c1 = b1-6.0*b2+45.0*c3;
    c0 = 1.0-b1+3.0*b2-15.0*c3;
    c = 0.1069/mu;
S30:
    if(g < 0.0) goto S50;
/*
             'SUBROUTINE' F IS CALLED (KFLAG=0 FOR CORRECT RETURN)
*/
    kflag = 0;
    goto S70;
S40:
/*
     STEP Q. QUOTIENT ACCEPTANCE (RARE CASE)
*/
    if(fy-u*fy <= py*exp(px-fx)) return ignpoi;
S50:
/*
     STEP E. EXPONENTIAL SAMPLE - SEXPO(IR) FOR STANDARD EXPONENTIAL
             DEVIATE E AND SAMPLE T FROM THE LAPLACE 'HAT'
             (IF T <= -.6744 THEN PK < FK FOR ALL MU >= 10.)
*/
    e = sexpo();
    u = ranf();
    u += (u-1.0);
    t = 1.8+fsign(e,u);
    if(t <= -0.6744) goto S50;
    ignpoi = (long) (mu+s*t);
    fk = (double)ignpoi;
    difmuk = mu-fk;
/*
             'SUBROUTINE' F IS CALLED (KFLAG=1 FOR CORRECT RETURN)
*/
    kflag = 1;
    goto S70;
S60:
/*
     STEP H. HAT ACCEPTANCE (E IS REPEATED ON REJECTION)
*/
    if(c*fabs(u) > py*exp(px+e)-fy*exp(fx+e)) goto S50;
    return ignpoi;
S70:
/*
     STEP F. 'SUBROUTINE' F. CALCULATION OF PX,PY,FX,FY.
             CASE IGNPOI .LT. 10 USES FACTORIALS FROM TABLE FACT
*/
    if(ignpoi >= 10) goto S80;
    px = -mu;
    py = pow(mu,(double)ignpoi)/ *(fact+ignpoi);
    goto S110;
S80:
/*
             CASE IGNPOI .GE. 10 USES POLYNOMIAL APPROXIMATION
             A0-A7 FOR ACCURACY WHEN ADVISABLE
             .8333333E-1=1./12.  .3989423=(2*PI)**(-.5)
*/
    del = 8.333333E-2/fk;
    del -= (4.8*del*del*del);
    v = difmuk/fk;
    if(fabs(v) <= 0.25) goto S90;
    px = fk*log(1.0+v)-difmuk-del;
    goto S100;
S90:
    px = fk*v*v*(((((((a7*v+a6)*v+a5)*v+a4)*v+a3)*v+a2)*v+a1)*v+a0)-del;
S100:
    py = 0.3989423/sqrt(fk);
S110:
    x = (0.5-difmuk)/s;
    xx = x*x;
    fx = -0.5*xx;
    fy = omega*(((c3*xx+c2)*xx+c1)*xx+c0);
    if(kflag <= 0) goto S40;
    goto S60;
S120:
/*
     C A S E  B. (START NEW TABLE AND CALCULATE P0 IF NECESSARY)
*/
    muprev = 0.0;
    if(mu == muold) goto S130;
    muold = mu;
    m = MAX(1L,(long) (mu));
    l = 0;
    p = exp(-mu);
    q = p0 = p;
S130:
/*
     STEP U. UNIFORM SAMPLE FOR INVERSION METHOD
*/
    u = ranf();
    ignpoi = 0;
    if(u <= p0) return ignpoi;
/*
     STEP T. TABLE COMPARISON UNTIL THE END PP(L) OF THE
             PP-TABLE OF CUMULATIVE POISSON PROBABILITIES
             (0.458=PP(9) FOR MU=10)
*/
    if(l == 0) goto S150;
    j = 1;
    if(u > 0.458) j = MIN(l,m);
    for(k=j; k<=l; k++) {
        if(u <= *(pp+k-1)) goto S180;
    }
    if(l == 35) goto S130;
S150:
/*
     STEP C. CREATION OF NEW POISSON PROBABILITIES P
             AND THEIR CUMULATIVES Q=PP(K)
*/
    l += 1;
    for(k=l; k<=35; k++) {
        p = p*mu/(double)k;
        q += p;
        *(pp+k-1) = q;
        if(u <= q) goto S170;
    }
    l = 35;
    goto S130;
S170:
    l = k;
S180:
    ignpoi = k;
    return ignpoi;
}
long lennob( char *str )
/* 
Returns the length of str ignoring trailing blanks but not 
other white space.
*/
{
long i, i_nb;

for (i=0, i_nb= -1L; *(str+i); i++)
    if ( *(str+i) != ' ' ) i_nb = i;
return (i_nb+1);
    }
long mltmod(long a,long s,long m)
/*
**********************************************************************
     long mltmod(long a,long s,long m)
                    Returns (A*S) MOD M
     This is a transcription from Pascal to Fortran of routine
     MULtMod_Decompos from the paper
     L'Ecuyer, P. and Cote, S. "Implementing a Random Number Package
     with Splitting Facilities." ACM Transactions on Mathematical
     Software, 17:98-111 (1991)
                              Arguments
     a, s, m  -->
**********************************************************************
*/
{
#define h 32768L
static long mltmod,a0,a1,k,p,q,qh,rh;
/*
     H = 2**((b-2)/2) where b = 32 because we are using a 32 bit
      machine. On a different machine recompute H
*/
    if(!(a <= 0 || a >= m || s <= 0 || s >= m)) goto S10;
    fputs(" a, m, s out of order in mltmod - ABORT!",stderr);
    fprintf(stderr," a = %12ld s = %12ld m = %12ld\n",a,s,m);
    fputs(" mltmod requires: 0 < a < m; 0 < s < m",stderr);
    exit(1);
S10:
    if(!(a < h)) goto S20;
    a0 = a;
    p = 0;
    goto S120;
S20:
    a1 = a/h;
    a0 = a-h*a1;
    qh = m/h;
    rh = m-h*qh;
    if(!(a1 >= h)) goto S50;
    a1 -= h;
    k = s/qh;
    p = h*(s-k*qh)-k*rh;
S30:
    if(!(p < 0)) goto S40;
    p += m;
    goto S30;
S40:
    goto S60;
S50:
    p = 0;
S60:
/*
     P = (A2*S*H)MOD M
*/
    if(!(a1 != 0)) goto S90;
    q = m/a1;
    k = s/q;
    p -= (k*(m-a1*q));
    if(p > 0) p -= m;
    p += (a1*(s-k*q));
S70:
    if(!(p < 0)) goto S80;
    p += m;
    goto S70;
S90:
S80:
    k = p/qh;
/*
     P = ((A2*H + A1)*S)MOD M
*/
    p = h*(p-k*qh)-k*rh;
S100:
    if(!(p < 0)) goto S110;
    p += m;
    goto S100;
S120:
S110:
    if(!(a0 != 0)) goto S150;
/*
     P = ((A2*H + A1)*H*S)MOD M
*/
    q = m/a0;
    k = s/q;
    p -= (k*(m-a0*q));
    if(p > 0) p -= m;
    p += (a0*(s-k*q));
S130:
    if(!(p < 0)) goto S140;
    p += m;
    goto S130;
S150:
S140:
    mltmod = p;
    return mltmod;
#undef h
}
void phrtsd(char* phrase,long *seed1,long *seed2)
/*
**********************************************************************
     void phrtsd(char* phrase,long *seed1,long *seed2)
               PHRase To SeeDs

                              Function

     Uses a phrase (character string) to generate two seeds for the RGN
     random number generator.
                              Arguments
     phrase --> Phrase to be used for random number generation
      
     seed1 <-- First seed for generator
                        
     seed2 <-- Second seed for generator
                        
                              Note

     Trailing blanks are eliminated before the seeds are generated.
     Generated seed values will fall in the range 1..2^30
     (1..1,073,741,824)
**********************************************************************
*/
{

static char table[] =
"abcdefghijklmnopqrstuvwxyz\
ABCDEFGHIJKLMNOPQRSTUVWXYZ\
0123456789\
!@#$%^&*()_+[];:'\\\"<>?,./";

long ix;

static long twop30 = 1073741824L;
static long shift[5] = {
    1L,64L,4096L,262144L,16777216L
};
static long i,ichr,j,lphr,values[5];
extern long lennob(char *str);

    *seed1 = 1234567890L;
    *seed2 = 123456789L;
    lphr = lennob(phrase); 
    if(lphr < 1) return;
    for(i=0; i<=(lphr-1); i++) {
	for (ix=0; table[ix]; ix++) if (*(phrase+i) == table[ix]) break; 
        if (!table[ix]) ix = 0;
        ichr = ix % 64;
        if(ichr == 0) ichr = 63;
        for(j=1; j<=5; j++) {
            *(values+j-1) = ichr-j;
            if(*(values+j-1) < 1) *(values+j-1) += 63;
        }
        for(j=1; j<=5; j++) {
            *seed1 = ( *seed1+*(shift+j-1)**(values+j-1) ) % twop30;
            *seed2 = ( *seed2+*(shift+j-1)**(values+6-j-1) )  % twop30;
        }
    }
#undef twop30
}
double ranf(void)
// NOTE(jsr) I changed this
{
    return dsfmt_genrand_close_open(&dsfmt);
}


double sdot(long n,double *sx,long incx,double *sy,long incy)
{
static long i,ix,iy,m,mp1;
static double sdot,stemp;
    stemp = sdot = 0.0;
    if(n <= 0) return sdot;
    if(incx == 1 && incy == 1) goto S20;
    ix = iy = 1;
    if(incx < 0) ix = (-n+1)*incx+1;
    if(incy < 0) iy = (-n+1)*incy+1;
    for(i=1; i<=n; i++) {
        stemp += (*(sx+ix-1)**(sy+iy-1));
        ix += incx;
        iy += incy;
    }
    sdot = stemp;
    return sdot;
S20:
    m = n % 5L;
    if(m == 0) goto S40;
    for(i=0; i<m; i++) stemp += (*(sx+i)**(sy+i));
    if(n < 5) goto S60;
S40:
    mp1 = m+1;
    for(i=mp1; i<=n; i+=5) stemp += (*(sx+i-1)**(sy+i-1)+*(sx+i)**(sy+i)+*(sx+i
      +1)**(sy+i+1)+*(sx+i+2)**(sy+i+2)+*(sx+i+3)**(sy+i+3));
S60:
    sdot = stemp;
    return sdot;
}
void spofa(double *a,long lda,long n,long *info)
/*
     SPOFA FACTORS A REAL SYMMETRIC POSITIVE DEFINITE MATRIX.
     SPOFA IS USUALLY CALLED BY SPOCO, BUT IT CAN BE CALLED
     DIRECTLY WITH A SAVING IN TIME IF  RCOND  IS NOT NEEDED.
     (TIME FOR SPOCO) = (1 + 18/N)*(TIME FOR SPOFA) .
     ON ENTRY
        A       REAL(LDA, N)
                THE SYMMETRIC MATRIX TO BE FACTORED.  ONLY THE
                DIAGONAL AND UPPER TRIANGLE ARE USED.
        LDA     INTEGER
                THE LEADING DIMENSION OF THE ARRAY  A .
        N       INTEGER
                THE ORDER OF THE MATRIX  A .
     ON RETURN
        A       AN UPPER TRIANGULAR MATRIX  R  SO THAT  A = TRANS(R)*R
                WHERE  TRANS(R)  IS THE TRANSPOSE.
                THE STRICT LOWER TRIANGLE IS UNALTERED.
                IF  INFO .NE. 0 , THE FACTORIZATION IS NOT COMPLETE.
        INFO    INTEGER
                = 0  FOR NORMAL RETURN.
                = K  SIGNALS AN ERROR CONDITION.  THE LEADING MINOR
                     OF ORDER  K  IS NOT POSITIVE DEFINITE.
     LINPACK.  THIS VERSION DATED 08/14/78 .
     CLEVE MOLER, UNIVERSITY OF NEW MEXICO, ARGONNE NATIONAL LAB.
     SUBROUTINES AND FUNCTIONS
     BLAS SDOT
     FORTRAN SQRT
     INTERNAL VARIABLES
*/
{
extern double sdot(long n,double *sx,long incx,double *sy,long incy);
static long j,jm1,k;
static double t,s;
/*
     BEGIN BLOCK WITH ...EXITS TO 40
*/
    for(j=1; j<=n; j++) {
        *info = j;
        s = 0.0;
        jm1 = j-1;
        if(jm1 < 1) goto S20;
        for(k=0; k<jm1; k++) {
            t = *(a+k+(j-1)*lda)-sdot(k,(a+k*lda),1L,(a+(j-1)*lda),1L);
            t /=  *(a+k+k*lda);
            *(a+k+(j-1)*lda) = t;
            s += (t*t);
        }
S20:
        s = *(a+j-1+(j-1)*lda)-s;
/*
     ......EXIT
*/
        if(s <= 0.0) goto S40;
        *(a+j-1+(j-1)*lda) = sqrt(s);
    }
    *info = 0;
S40:
    return;
}

void setgmn(double *meanv,double *covm,long p,double *parm)
/*
**********************************************************************
     void setgmn(double *meanv,double *covm,long p,double *parm)
            SET Generate Multivariate Normal random deviate
                              Function
      Places P, MEANV, and the Cholesky factoriztion of COVM
      in GENMN.
                              Arguments
     meanv --> Mean vector of multivariate normal distribution.
     covm   <--> (Input) Covariance   matrix    of  the  multivariate
                 normal distribution
                 (Output) Destroyed on output
     p     --> Dimension of the normal, or length of MEANV.
     parm <-- Array of parameters needed to generate multivariate norma
                deviates (P, MEANV and Cholesky decomposition of
                COVM).
                1 : 1                - P
                2 : P + 1            - MEANV
                P+2 : P*(P+3)/2 + 1  - Cholesky decomposition of COVM
               Needed dimension is (p*(p+3)/2 + 1)
**********************************************************************
*/
{
static long T1;
static long i,icount,info,j,D2,D3,D4,D5;
    T1 = p*(p+3)/2+1;
/*
     TEST THE INPUT
*/
    if(!(p <= 0)) goto S10;
    fputs("P nonpositive in SETGMN",stderr);
    fprintf(stderr,"Value of P: %12ld\n",p);
    exit(1);
S10:
    *parm = p;
/*
     PUT P AND MEANV INTO PARM
*/
    for(i=2,D2=1,D3=(p+1-i+D2)/D2; D3>0; D3--,i+=D2) *(parm+i-1) = *(meanv+i-2);
/*
      Cholesky decomposition to find A s.t. trans(A)*(A) = COVM
*/
    spofa(covm,p,p,&info);
    if(!(info != 0)) goto S30;
    fputs(" COVM not positive definite in SETGMN",stderr);
    exit(1);
S30:
    icount = p+1;
/*
     PUT UPPER HALF OF A, WHICH IS NOW THE CHOLESKY FACTOR, INTO PARM
          COVM(1,1) = PARM(P+2)
          COVM(1,2) = PARM(P+3)
                    :
          COVM(1,P) = PARM(2P+1)
          COVM(2,2) = PARM(2P+2)  ...
*/
    for(i=1,D4=1,D5=(p-i+D4)/D4; D5>0; D5--,i+=D4) {
        for(j=i-1; j<p; j++) {
            icount += 1;
            *(parm+icount-1) = *(covm+i-1+j*p);
        }
    }
}
double sexpo(void)
/*
**********************************************************************
                                                                      
                                                                      
     (STANDARD-)  E X P O N E N T I A L   DISTRIBUTION                
                                                                      
                                                                      
**********************************************************************
**********************************************************************
                                                                      
     FOR DETAILS SEE:                                                 
                                                                      
               AHRENS, J.H. AND DIETER, U.                            
               COMPUTER METHODS FOR SAMPLING FROM THE                 
               EXPONENTIAL AND NORMAL DISTRIBUTIONS.                  
               COMM. ACM, 15,10 (OCT. 1972), 873 - 882.               
                                                                      
     ALL STATEMENT NUMBERS CORRESPOND TO THE STEPS OF ALGORITHM       
     'SA' IN THE ABOVE PAPER (SLIGHTLY MODIFIED IMPLEMENTATION)       
                                                                      
     Modified by Barry W. Brown, Feb 3, 1988 to use RANF instead of   
     SUNIF.  The argument IR thus goes away.                          
                                                                      
**********************************************************************
     Q(N) = SUM(ALOG(2.0)**K/K!)    K=1,..,N ,      THE HIGHEST N
     (HERE 8) IS DETERMINED BY Q(N)=1.0 WITHIN STANDARD PRECISION
*/
{
static double q[8] = {
    0.6931472,0.9333737,0.9888778,0.9984959,0.9998293,0.9999833,0.9999986,1.0
};
static long i;
static double sexpo,a,u,ustar,umin;
static double *q1 = q;
    a = 0.0;
    u = ranf();
    goto S30;
S20:
    a += *q1;
S30:
    u += u;
    if(u <= 1.0) goto S20;
    u -= 1.0;
    if(u > *q1) goto S60;
    sexpo = a+u;
    return sexpo;
S60:
    i = 1;
    ustar = ranf();
    umin = ustar;
S70:
    ustar = ranf();
    if(ustar < umin) umin = ustar;
    i += 1;
    if(u > *(q+i-1)) goto S70;
    sexpo = a+umin**q1;
    return sexpo;
}
double sgamma(double a)
/*
**********************************************************************
                                                                      
                                                                      
     (STANDARD-)  G A M M A  DISTRIBUTION                             
                                                                      
                                                                      
**********************************************************************
**********************************************************************
                                                                      
               PARAMETER  A >= 1.0  !                                 
                                                                      
**********************************************************************
                                                                      
     FOR DETAILS SEE:                                                 
                                                                      
               AHRENS, J.H. AND DIETER, U.                            
               GENERATING GAMMA VARIATES BY A                         
               MODIFIED REJECTION TECHNIQUE.                          
               COMM. ACM, 25,1 (JAN. 1982), 47 - 54.                  
                                                                      
     STEP NUMBERS CORRESPOND TO ALGORITHM 'GD' IN THE ABOVE PAPER     
                                 (STRAIGHTFORWARD IMPLEMENTATION)     
                                                                      
     Modified by Barry W. Brown, Feb 3, 1988 to use RANF instead of   
     SUNIF.  The argument IR thus goes away.                          
                                                                      
**********************************************************************
                                                                      
               PARAMETER  0.0 < A < 1.0  !                            
                                                                      
**********************************************************************
                                                                      
     FOR DETAILS SEE:                                                 
                                                                      
               AHRENS, J.H. AND DIETER, U.                            
               COMPUTER METHODS FOR SAMPLING FROM GAMMA,              
               BETA, POISSON AND BINOMIAL DISTRIBUTIONS.              
               COMPUTING, 12 (1974), 223 - 246.                       
                                                                      
     (ADAPTED IMPLEMENTATION OF ALGORITHM 'GS' IN THE ABOVE PAPER)    
                                                                      
**********************************************************************
     INPUT: A =PARAMETER (MEAN) OF THE STANDARD GAMMA DISTRIBUTION
     OUTPUT: SGAMMA = SAMPLE FROM THE GAMMA-(A)-DISTRIBUTION
     COEFFICIENTS Q(K) - FOR Q0 = SUM(Q(K)*A**(-K))
     COEFFICIENTS A(K) - FOR Q = Q0+(T*T/2)*SUM(A(K)*V**K)
     COEFFICIENTS E(K) - FOR EXP(Q)-1 = SUM(E(K)*Q**K)
     PREVIOUS A PRE-SET TO ZERO - AA IS A', AAA IS A"
     SQRT32 IS THE SQUAREROOT OF 32 = 5.656854249492380
*/
{
extern double fsign( double num, double sign );
static double q1 = 4.166669E-2;
static double q2 = 2.083148E-2;
static double q3 = 8.01191E-3;
static double q4 = 1.44121E-3;
static double q5 = -7.388E-5;
static double q6 = 2.4511E-4;
static double q7 = 2.424E-4;
static double a1 = 0.3333333;
static double a2 = -0.250003;
static double a3 = 0.2000062;
static double a4 = -0.1662921;
static double a5 = 0.1423657;
static double a6 = -0.1367177;
static double a7 = 0.1233795;
static double e1 = 1.0;
static double e2 = 0.4999897;
static double e3 = 0.166829;
static double e4 = 4.07753E-2;
static double e5 = 1.0293E-2;
static double aa = 0.0;
static double aaa = 0.0;
static double sqrt32 = 5.656854;
static double sgamma,s2,s,d,t,x,u,r,q0,b,si,c,v,q,e,w,p;
    if(a == aa) goto S10;
    if(a < 1.0) goto S120;
/*
     STEP  1:  RECALCULATIONS OF S2,S,D IF A HAS CHANGED
*/
    aa = a;
    s2 = a-0.5;
    s = sqrt(s2);
    d = sqrt32-12.0*s;
S10:
/*
     STEP  2:  T=STANDARD NORMAL DEVIATE,
               X=(S,1/2)-NORMAL DEVIATE.
               IMMEDIATE ACCEPTANCE (I)
*/
    t = snorm();
    x = s+0.5*t;
    sgamma = x*x;
    if(t >= 0.0) return sgamma;
/*
     STEP  3:  U= 0,1 -UNIFORM SAMPLE. SQUEEZE ACCEPTANCE (S)
*/
    u = ranf();
    if(d*u <= t*t*t) return sgamma;
/*
     STEP  4:  RECALCULATIONS OF Q0,B,SI,C IF NECESSARY
*/
    if(a == aaa) goto S40;
    aaa = a;
    r = 1.0/ a;
    q0 = ((((((q7*r+q6)*r+q5)*r+q4)*r+q3)*r+q2)*r+q1)*r;
/*
               APPROXIMATION DEPENDING ON SIZE OF PARAMETER A
               THE CONSTANTS IN THE EXPRESSIONS FOR B, SI AND
               C WERE ESTABLISHED BY NUMERICAL EXPERIMENTS
*/
    if(a <= 3.686) goto S30;
    if(a <= 13.022) goto S20;
/*
               CASE 3:  A .GT. 13.022
*/
    b = 1.77;
    si = 0.75;
    c = 0.1515/s;
    goto S40;
S20:
/*
               CASE 2:  3.686 .LT. A .LE. 13.022
*/
    b = 1.654+7.6E-3*s2;
    si = 1.68/s+0.275;
    c = 6.2E-2/s+2.4E-2;
    goto S40;
S30:
/*
               CASE 1:  A .LE. 3.686
*/
    b = 0.463+s+0.178*s2;
    si = 1.235;
    c = 0.195/s-7.9E-2+1.6E-1*s;
S40:
/*
     STEP  5:  NO QUOTIENT TEST IF X NOT POSITIVE
*/
    if(x <= 0.0) goto S70;
/*
     STEP  6:  CALCULATION OF V AND QUOTIENT Q
*/
    v = t/(s+s);
    if(fabs(v) <= 0.25) goto S50;
    q = q0-s*t+0.25*t*t+(s2+s2)*log(1.0+v);
    goto S60;
S50:
    q = q0+0.5*t*t*((((((a7*v+a6)*v+a5)*v+a4)*v+a3)*v+a2)*v+a1)*v;
S60:
/*
     STEP  7:  QUOTIENT ACCEPTANCE (Q)
*/
    if(log(1.0-u) <= q) return sgamma;
S70:
/*
     STEP  8:  E=STANDARD EXPONENTIAL DEVIATE
               U= 0,1 -UNIFORM DEVIATE
               T=(B,SI)-DOUBLE EXPONENTIAL (LAPLACE) SAMPLE
*/
    e = sexpo();
    u = ranf();
    u += (u-1.0);
    t = b+fsign(si*e,u);
/*
     STEP  9:  REJECTION IF T .LT. TAU(1) = -.71874483771719
*/
    if(t < -0.7187449) goto S70;
/*
     STEP 10:  CALCULATION OF V AND QUOTIENT Q
*/
    v = t/(s+s);
    if(fabs(v) <= 0.25) goto S80;
    q = q0-s*t+0.25*t*t+(s2+s2)*log(1.0+v);
    goto S90;
S80:
    q = q0+0.5*t*t*((((((a7*v+a6)*v+a5)*v+a4)*v+a3)*v+a2)*v+a1)*v;
S90:
/*
     STEP 11:  HAT ACCEPTANCE (H) (IF Q NOT POSITIVE GO TO STEP 8)
*/
    if(q <= 0.0) goto S70;
    if(q <= 0.5) goto S100;
    w = exp(q)-1.0;
    goto S110;
S100:
    w = ((((e5*q+e4)*q+e3)*q+e2)*q+e1)*q;
S110:
/*
               IF T IS REJECTED, SAMPLE AGAIN AT STEP 8
*/
    if(c*fabs(u) > w*exp(e-0.5*t*t)) goto S70;
    x = s+0.5*t;
    sgamma = x*x;
    return sgamma;
S120:
/*
     ALTERNATE METHOD FOR PARAMETERS A BELOW 1  (.3678794=EXP(-1.))
*/
    aa = 0.0;
    b = 1.0+0.3678794*a;
S130:
    p = b*ranf();
    if(p >= 1.0) goto S140;
    sgamma = exp(log(p)/ a);
    if(sexpo() < sgamma) goto S130;
    return sgamma;
S140:
    sgamma = -log((b-p)/ a);
    if(sexpo() < (1.0-a)*log(sgamma)) goto S130;
    return sgamma;
}
double snorm(void)
/*
**********************************************************************
                                                                      
                                                                      
     (STANDARD-)  N O R M A L  DISTRIBUTION                           
                                                                      
                                                                      
**********************************************************************
**********************************************************************
                                                                      
     FOR DETAILS SEE:                                                 
                                                                      
               AHRENS, J.H. AND DIETER, U.                            
               EXTENSIONS OF FORSYTHE'S METHOD FOR RANDOM             
               SAMPLING FROM THE NORMAL DISTRIBUTION.                 
               MATH. COMPUT., 27,124 (OCT. 1973), 927 - 937.          
                                                                      
     ALL STATEMENT NUMBERS CORRESPOND TO THE STEPS OF ALGORITHM 'FL'  
     (M=5) IN THE ABOVE PAPER     (SLIGHTLY MODIFIED IMPLEMENTATION)  
                                                                      
     Modified by Barry W. Brown, Feb 3, 1988 to use RANF instead of   
     SUNIF.  The argument IR thus goes away.                          
                                                                      
**********************************************************************
     THE DEFINITIONS OF THE CONSTANTS A(K), D(K), T(K) AND
     H(K) ARE ACCORDING TO THE ABOVEMENTIONED ARTICLE
*/
{
static double a[32] = {
    0.0,3.917609E-2,7.841241E-2,0.11777,0.1573107,0.1970991,0.2372021,0.2776904,
    0.3186394,0.36013,0.4022501,0.4450965,0.4887764,0.5334097,0.5791322,
    0.626099,0.6744898,0.7245144,0.7764218,0.8305109,0.8871466,0.9467818,
    1.00999,1.077516,1.150349,1.229859,1.318011,1.417797,1.534121,1.67594,
    1.862732,2.153875
};
static double d[31] = {
    0.0,0.0,0.0,0.0,0.0,0.2636843,0.2425085,0.2255674,0.2116342,0.1999243,
    0.1899108,0.1812252,0.1736014,0.1668419,0.1607967,0.1553497,0.1504094,
    0.1459026,0.14177,0.1379632,0.1344418,0.1311722,0.128126,0.1252791,
    0.1226109,0.1201036,0.1177417,0.1155119,0.1134023,0.1114027,0.1095039
};
static double t[31] = {
    7.673828E-4,2.30687E-3,3.860618E-3,5.438454E-3,7.0507E-3,8.708396E-3,
    1.042357E-2,1.220953E-2,1.408125E-2,1.605579E-2,1.81529E-2,2.039573E-2,
    2.281177E-2,2.543407E-2,2.830296E-2,3.146822E-2,3.499233E-2,3.895483E-2,
    4.345878E-2,4.864035E-2,5.468334E-2,6.184222E-2,7.047983E-2,8.113195E-2,
    9.462444E-2,0.1123001,0.136498,0.1716886,0.2276241,0.330498,0.5847031
};
static double h[31] = {
    3.920617E-2,3.932705E-2,3.951E-2,3.975703E-2,4.007093E-2,4.045533E-2,
    4.091481E-2,4.145507E-2,4.208311E-2,4.280748E-2,4.363863E-2,4.458932E-2,
    4.567523E-2,4.691571E-2,4.833487E-2,4.996298E-2,5.183859E-2,5.401138E-2,
    5.654656E-2,5.95313E-2,6.308489E-2,6.737503E-2,7.264544E-2,7.926471E-2,
    8.781922E-2,9.930398E-2,0.11556,0.1404344,0.1836142,0.2790016,0.7010474
};
static long i;
static double snorm,u,s,ustar,aa,w,y,tt;
    u = ranf();
    s = 0.0;
    if(u > 0.5) s = 1.0;
    u += (u-s);
    u = 32.0*u;
    i = (long) (u);
    if(i == 32) i = 31;
    if(i == 0) goto S100;
/*
                                START CENTER
*/
    ustar = u-(double)i;
    aa = *(a+i-1);
S40:
    if(ustar <= *(t+i-1)) goto S60;
    w = (ustar-*(t+i-1))**(h+i-1);
S50:
/*
                                EXIT   (BOTH CASES)
*/
    y = aa+w;
    snorm = y;
    if(s == 1.0) snorm = -y;
    return snorm;
S60:
/*
                                CENTER CONTINUED
*/
    u = ranf();
    w = u*(*(a+i)-aa);
    tt = (0.5*w+aa)*w;
    goto S80;
S70:
    tt = u;
    ustar = ranf();
S80:
    if(ustar > tt) goto S50;
    u = ranf();
    if(ustar >= u) goto S70;
    ustar = ranf();
    goto S40;
S100:
/*
                                START TAIL
*/
    i = 6;
    aa = *(a+31);
    goto S120;
S110:
    aa += *(d+i-1);
    i += 1;
S120:
    u += u;
    if(u < 1.0) goto S110;
    u -= 1.0;
S140:
    w = u**(d+i-1);
    tt = (0.5*w+aa)*w;
    goto S160;
S150:
    tt = u;
S160:
    ustar = ranf();
    if(ustar > tt) goto S50;
    u = ranf();
    if(ustar >= u) goto S150;
    u = ranf();
    goto S140;
}
double fsign( double num, double sign )
/* Transfers sign of argument sign to argument num */
{
if ( ( sign>0.0f && num<0.0f ) || ( sign<0.0f && num>0.0f ) )
    return -num;
else return num;
}




////////////////////////////////////////////////////////////////////////////
// From here on is stuff I've added




template <typename vec_t>
void normalizeLog(vec_t& x) {
    long double s;
    int i;
    s = 0;

    long double normalized_sum = 0;

    for (i = 0; i < x.size(); i++) {
        s = addLog(s, x[i]);
    }
    for (i = 0; i < x.size(); i++) {
        x[i] = exp(x[i] - s);
        normalized_sum += x[i];
    }

    // CHECK(MathUtil::NearByMargin(normalized_sum,1.0));
    // LOG(INFO) << "normalized sum " << normalized_sum;
    CHECK_GT(normalized_sum, 0);  // for nan
    CHECK_LT(fabs(normalized_sum - 1.0), epsilonValue);
}

inline double sample_uniform() {
    return ranf();
}

template <typename vec_t>
inline int sample_multinomial(const vec_t& d) {
    double cut = ranf();
    CHECK_LE(cut, 1.0);
    CHECK_GE(cut, 0.0);

    for (int i = 0; i < d.size(); i++) {
        cut -= d[i];

        if (cut < 0) {
            return i;
        }
    }

    CHECK(false) << "improperly normalized distribution " << cut;
    return 0;
}

// Assume that the data coming in are log probs and that they need to be
// appropriately normalized.
// XXX: sample_log_multinomial changes d into normal p space
template <typename vec_t>
int sample_log_multinomial(vec_t& d) {
    normalizeLog(d);
    return sample_multinomial(d);
}


inline double sample_normal() {
    double x1, x2, w, y1;

    static bool returned = false;
    static double y2 = 0.0;

    if (returned) {
        returned = false;
        do {
            x1 = 2.0 * ranf() - 1.0;
            x2 = 2.0 * ranf() - 1.0;
            w = x1 * x1 + x2 * x2;
        } while ( w >= 1.0 );

        w = sqrt((-2.0 * log(w)) / w);
        y1 = x1 * w;
        y2 = x2 * w;
        return y1;
    } else {
        returned = true;
        return y2;
    }
}

inline double sample_gaussian(double mu, double si2) {
    return si2*sample_normal()+mu;
}

// Generate a vector of samples from the same gaussian
ublas::vector<double> sample_gaussian_vector(double mu, double si2, size_t dim) {
    ublas::vector<double> result(dim);
    for (int i = 0; i < dim; i++) {
        result[i] = si2*sample_gaussian()+mu;
    }
    return result;
}

// Sample from a Gaussian (spherical) and then normalize the resulting draw onto
// the unit hypersphere
template <typename vec_t>
ublas::vector<double> sample_spherical_gaussian(const vec_t& mean, double si2) {
    ublas::vector<double> result(mean.size());
    for (int i = 0; i < mean.size(); i++) {
        result[i] = si2*sample_normal()+mean[i];
    }
    return result / norm_2(result);
}

double sample_gamma(double a, double b) {
    return BOUNDLO(gengam(1/b,a));
}

template <typename vec_t>
ublas::vector<double> sample_vmf(const vec_t& mu, double kappa) {
    CHECK_GT(mu.size(), 2);  // Otherwise we don't need 'Fisher'
    unsigned d = mu.size();

    // draw a sample
    double t1 = sqrt(4.0 * kappa * kappa + (d - 1) * (d - 1));
    double b = (-2 * kappa + t1) / (d - 1);
    double x0 = (1 - b) / (1 + b);
    double m = (d - 1) / 2.0;
    double c = kappa * x0 + (d - 1) * log(1 - x0 * x0);
    double t = -1000;
    double u = 1.0;
    double w = 0.0;

    while (t < log(u)) {
        // gather the underlying random variates
        double z = genbet(m, m);  // Generate a beta variate
        u = sample_uniform();
        w = (1 - (1 + b) * z) / (1 - (1 - b) * z);

        // this line is wrong in (Dhillon and Sra, 2003)
        t = kappa * w + (d - 1) * log(1 - x0 * w) - c;
    }

    // generate a vMF-distributed vector about the zeroth axis
    // this part makes a uniform unit vector (randomly distributed)
    ublas::vector<double> result(d);
    for (int i = 0; i < d; i++) {
        if (i < d-1) {
            result[i] = sample_uniform()*2.0-1.0;
        } else {
            result[i] = 0.0;
        }
    }
    double norm = norm_2(result);
    for (int i = 0; i < d; i++) {
        result[i] /= norm;
        result[i] *= sqrt(1-w*w);
    }
    result[d-1] = w;

    // rotate the random vector to distribute around the mean
    ublas::matrix<double> xxx = rotation_matrix(ublas::unit_vector<double>(d,d-1), mu);
    LOG(INFO) <<"pease";
    return prod(rotation_matrix(ublas::unit_vector<double>(d,d-1), mu), result);
}

// Return the matrix reflecting unit vector C{p} to unit vector C{q}.
ublas::matrix<double> reflection_matrix(const ublas::vector<double>& p, const ublas::vector<double>& q) {
    CHECK_EQ(p.size(), q.size());
    ublas::vector<double> pq = p - q;
    double npq = norm_2(pq);
    unsigned n = p.size();

    if (npq < epsilonValue) {
        return ublas::identity_matrix<double>(n);
    } else {
        pq /= npq;
        // So the outer product below blows up when |V| > 65K, we need to up the
        // allocator size type
        // LOG(INFO) << std::numeric_limits<long long unsigned>::max() << "   " << sqrt(std::numeric_limits<long long unsigned>::max());
        return ublas::identity_matrix<double>(n) - 2.0 * outer_prod(pq, pq);
    }
}

// Return the matrix rotating unit vector C{f} to unit vector C{t}.
ublas::matrix<double> rotation_matrix(const ublas::vector<double>& f, const ublas::vector<double>& t) {
    // TODO(jsr) isn't this already implemented in BLAS somewhere? like _rot ?
    if (norm_2(f-t) < epsilonValue) {
        return ublas::identity_matrix<double>(f.size());
    } else {
        LOG(INFO) << "cats";
        ublas::matrix<double> a = reflection_matrix(t, -f);
        LOG(INFO) << "cats";
        ublas::matrix<double> b = reflection_matrix(f, -f);
        LOG(INFO) << "cats";
        return prod(reflection_matrix(t, -f), reflection_matrix(f, -f));
    }
}



ublas::vector<double> sample_sym_dirichlet(double alpha, unsigned dim) {
    int i;
    double s;
    s = 0;
    ublas::vector<double> result(dim);
    for (i=0; i<dim; i++) {
        result[i] = BOUNDLO(gengam(1.0,alpha));
        s += result[i];
    }
    for (i=0; i<dim; i++) { result[i] = BOUND(result[i]/s); }
    
    return result;
}

// if we currently have K clusters, then pi is a distribution over K+1
// items, which means that it's a vector of length K+2 (the last
// position is used to store the sum of pi).  counts will be a vector
// over K+1 items, which means it will also have K+2 elements.
void   sample_PY(double*pi, double alpha, double delta, int K, double*counts, int*Knew, double**pinew) {
  // this is just like a dirichlet sample, but with (a) discounting
  // and (b) variable number of clusters we end up sampling from
  // Dir(Z, K+1), where Z(k) = (alpha+delta*K)/Kunused if k is unused
  // and = counts(k) - delta, if k is used.  Kunused in the number of
  // unused clusters finally, if everything is used, we need to add a
  // single unused slot
  int Kunused;
  int k;
  double s;
  double*oldpi;

  // how many unused slots are there?
  Kunused = 0;
  for (k=0; k<K+1; k++) {
    if (counts[k] <= 0) { Kunused++; }
  }

  if (Kunused > 0) {  // there are unused slots: this is the easy case
    // store the posterior in "pi"
    s = 0;
    for (k=0; k<K+1; k++) {
      if (counts[k] > 0) {
        pi[k] = counts[k] - delta;
      } else {
        pi[k] = (alpha + delta * (double)(K-Kunused)) / (double)Kunused;
      }
      s += pi[k];
    }
    pi[K+1] = s;  // store sum

    // set return values
    *Knew = K;
    *pinew = pi;
  } else {
    // if there are no unused slots, make one!
    oldpi = pi;
    pi = (double*) realloc(pi, (K+3)*sizeof(double));
    //fprintf(stderr, "reallocating pi to %d\t(%p to %p)\n", K+3, oldpi, pi);
    if (!pi) { fprintf(stderr, "accckkk!"); }
    pi[K+2] = 0;

    // store posterior in pi
    s = 0;
    for (k=0; k<K+1; k++) {
      pi[k] = counts[k] - delta;
      s += pi[k];
    }
    pi[K+1] = (alpha + delta * (double)K);
    pi[K+2] = s + pi[K+1];  // store sum

    // set return values
    *Knew = K+1;
    *pinew = pi;
  }
}


// Computes the log prob of a value in a symmetric dirichlet
template <typename vec_t>
double logp_sym_dirichlet(const vec_t& value, double alpha) {
    unsigned dim = value.size();
    DCHECK_LT(fabs(sum(value) - 1.0), epsilonValue);
    double l;
    double s;
    // TODO: we shouldn't have to compute the sum
    double alpha_sum = alpha*((double)dim);
    // s = th[dim];
    // l = 0;
    // if (normalize) {
    l = gammaln(alpha_sum) - ((double)dim) * gammaln(alpha);
    // }
    //for (i=0;i<dim;i++) { s += th[i]; }
    // for (i=0;i<dim;i++) { l += (al-1) * log(BOUND(th[i]/s)); }
    for (int i=0;i<dim;i++) { l += (alpha-1) * log(BOUND(value[i])); }
    return BOUNDPROB(l);
}

double logp_gamma(double x, double a, double b) {
    if (x < 0) { return NEGINF; }
    // return BOUNDPROB((normalize ? (-a*log(b) - gammaln(a)) : (0.0)) + (a-1)*log(x) - x/b);
    return BOUNDPROB(-a*log(b) - gammaln(a) + (a-1)*log(x) - x/b);
}

// For now always assume mu and v can differ in underlying vector representation
template <typename vec_t1, typename vec_t2>
double logp_vmf(const vec_t1& v, const vec_t2& mu, double kappa, bool normalize) {
    CHECK_EQ(v.size(), mu.size());
    // TODO(jsr) figure out how to do this properly
    DCHECK_LT(fabs(1.0-norm_2(v)), epsilonValue);
    DCHECK_LT(fabs(1.0-norm_2(mu)), epsilonValue);

    unsigned p = v.size();

    if (normalize) {
        // Compute an approximate log modified bessel function of the first kind
        double l_bessel = approx_log_iv(p / 2.0 - 1.0, kappa);

        return kappa * inner_prod(mu, v) 
               + (p/2.0)*log(kappa / (2*M_PI)) 
               - log(kappa) 
               - l_bessel;
    } else {  // in the unnormalized case, don't compute terms only involving kappa
        return kappa * inner_prod(mu, v);
    }
}


// Computes the Abramowitz and Stegum approximation to the log modified bessel
// funtion of the first kind -- stable for high values of nu. See Chris Elkan's
double approx_log_iv(double nu, double z) {
    double alpha = 1 + pow(z / nu, 2);
    double eta = sqrt(alpha) + log(z / nu) - log(1+sqrt(alpha));
    return -log(sqrt(2*M_PI*nu)) + nu*eta - 0.25 * log(alpha);
}