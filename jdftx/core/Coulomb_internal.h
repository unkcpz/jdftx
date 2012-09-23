/*-------------------------------------------------------------------
Copyright 2012 Ravishankar Sundararaman

This file is part of JDFTx.

JDFTx is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

JDFTx is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with JDFTx.  If not, see <http://www.gnu.org/licenses/>.
-------------------------------------------------------------------*/

#ifndef JDFTX_CORE_COULOMB_INTERNAL_H
#define JDFTX_CORE_COULOMB_INTERNAL_H

//! @file Coulomb_internal.h Shared inline functions for anlaytical truncated Coulomb kernels

#include <core/matrix3.h>
#include <core/Bspline.h>

//! Periodic coulomb interaction (4 pi/G^2)
struct CoulombPeriodic_calc
{	__hostanddev__ double operator()(const vector3<int>& iG, const matrix3<>& GGT) const
	{	double Gsq = GGT.metric_length_squared(iG);
		return Gsq ? (4*M_PI)/Gsq : 0.;
	}
};

//! Slab-truncated coulomb interaction
struct CoulombSlab_calc
{	int iDir; double hlfL;
	CoulombSlab_calc(int iDir, double hlfL) : iDir(iDir), hlfL(hlfL) {}
	__hostanddev__ double operator()(const vector3<int>& iG, const matrix3<>& GGT) const
	{	double Gsq = GGT.metric_length_squared(iG);
		double Gplane = Gsq - GGT(iDir,iDir) * iG[iDir]*iG[iDir]; //G along the non-truncated directions
		Gplane = Gplane>0. ? sqrt(Gplane) : 0.; //safe sqrt to prevent NaN from roundoff errors
		return (4*M_PI) * (Gsq ? (1. - exp(-Gplane*hlfL) * cos(M_PI*iG[iDir]))/Gsq : -0.5*hlfL*hlfL);
	}
};

//! Sphere-truncated coulomb interaction
struct CoulombSpherical_calc
{	double Rc;
	CoulombSpherical_calc(double Rc) : Rc(Rc) {}
	__hostanddev__ double operator()(const vector3<int>& iG, const matrix3<>& GGT) const
	{	double Gsq = GGT.metric_length_squared(iG);
		return Gsq ? (4*M_PI) * (1. - cos(Rc*sqrt(Gsq)))/Gsq : (2*M_PI)*Rc*Rc;
	}
};

#ifdef GPU_ENABLED
void coulombAnalytic_gpu(vector3<int> S, const matrix3<>& GGT, const CoulombPeriodic_calc& calc, complex* data);
void coulombAnalytic_gpu(vector3<int> S, const matrix3<>& GGT, const CoulombSlab_calc& calc, complex* data);
void coulombAnalytic_gpu(vector3<int> S, const matrix3<>& GGT, const CoulombSpherical_calc& calc, complex* data);
#endif
void coulombAnalytic(vector3<int> S, const matrix3<>& GGT, const CoulombPeriodic_calc& calc, complex* data);
void coulombAnalytic(vector3<int> S, const matrix3<>& GGT, const CoulombSlab_calc& calc, complex* data);
void coulombAnalytic(vector3<int> S, const matrix3<>& GGT, const CoulombSpherical_calc& calc, complex* data);

//! Compute erf(x)/x (with x~0 handled properly)
__hostanddev__ double erf_by_x(double x)
{	double xSq = x*x;
	if(xSq<1e-6) return (1./sqrt(M_PI))*(2. - xSq*(2./3 + 0.2*xSq));
	else return erf(x)/x;
}

//---------------------- Exchange Kernels --------------------

//In each of the following functions, kSq is the square of the appropriate
//wave vector (includes reciprocal lattice vector and k-point difference),
//and will not be zero (the G=0 term is handled in the calling routine)

//! Radial fourier transform of erfc(omega r)/r (not valid at G=0)
__hostanddev__ double erfcTilde(double Gsq, double omegaSq)
{	return (4*M_PI) * (omegaSq ? (1.-exp(-0.25*Gsq/omegaSq)) : 1.) / Gsq;
}


//! Periodic exchange
struct ExchangePeriodic_calc
{	__hostanddev__ double operator()(double kSq) const
	{	return (4*M_PI) / kSq;
	}
};

//! Erfc-screened Periodic exchange
struct ExchangePeriodicScreened_calc
{	double inv4omegaSq; //!< 1/(4 omega^2)
	ExchangePeriodicScreened_calc(double omega) : inv4omegaSq(0.25/(omega*omega)) {}
	
	__hostanddev__ double operator()(double kSq) const
	{	return (4*M_PI) * (1.-exp(-inv4omegaSq*kSq)) / kSq;
	}
};

//! Spherical-truncated exchange
struct ExchangeSpherical_calc
{	double Rc;
	ExchangeSpherical_calc(double Rc) : Rc(Rc) {}
	
	__hostanddev__ double operator()(double kSq) const
	{	return (4*M_PI) * (1. - cos(Rc * sqrt(kSq))) / kSq;
	}
};

//! Erfc-screened Spherical-truncated exchange
struct ExchangeSphericalScreened_calc
{	const double* coeff; //!< quintic spline coefficients
	double dGinv; //!< inverse of coefficient spacing
	size_t nSamples; //!< number of coefficients
	ExchangeSphericalScreened_calc(const double* coeff, double dGinv, size_t nSamples)
	: coeff(coeff), dGinv(dGinv), nSamples(nSamples) {}
	
	__hostanddev__ double operator()(double kSq) const
	{	double t = dGinv * sqrt(kSq);
		if(t >= nSamples) return 0.;
		else return QuinticSpline::value(coeff, t);
	}
};

void exchangeAnalytic(vector3<int> S, const matrix3<>& GGT, const ExchangePeriodic_calc& calc, complex* data, const vector3<>& kDiff, double Vzero, double thresholdSq);
void exchangeAnalytic(vector3<int> S, const matrix3<>& GGT, const ExchangePeriodicScreened_calc& calc, complex* data, const vector3<>& kDiff, double Vzero, double thresholdSq);
void exchangeAnalytic(vector3<int> S, const matrix3<>& GGT, const ExchangeSpherical_calc& calc, complex* data, const vector3<>& kDiff, double Vzero, double thresholdSq);
void exchangeAnalytic(vector3<int> S, const matrix3<>& GGT, const ExchangeSphericalScreened_calc& calc, complex* data, const vector3<>& kDiff, double Vzero, double thresholdSq);
#ifdef GPU_ENABLED
void exchangeAnalytic_gpu(vector3<int> S, const matrix3<>& GGT, const ExchangePeriodic_calc& calc, complex* data, const vector3<>& kDiff, double Vzero, double thresholdSq);
void exchangeAnalytic_gpu(vector3<int> S, const matrix3<>& GGT, const ExchangePeriodicScreened_calc& calc, complex* data, const vector3<>& kDiff, double Vzero, double thresholdSq);
void exchangeAnalytic_gpu(vector3<int> S, const matrix3<>& GGT, const ExchangeSpherical_calc& calc, complex* data, const vector3<>& kDiff, double Vzero, double thresholdSq);
void exchangeAnalytic_gpu(vector3<int> S, const matrix3<>& GGT, const ExchangeSphericalScreened_calc& calc, complex* data, const vector3<>& kDiff, double Vzero, double thresholdSq);
#endif

#endif // JDFTX_CORE_COULOMB_INTERNAL_H