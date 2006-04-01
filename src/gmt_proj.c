/*--------------------------------------------------------------------
 *	$Id: gmt_proj.c,v 1.3 2006-04-01 23:50:51 pwessel Exp $
 *
 *	Copyright (c) 1991-2006 by P. Wessel and W. H. F. Smith
 *	See COPYING file for copying and redistribution conditions.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	Contact info: gmt.soest.hawaii.edu
 *--------------------------------------------------------------------*/
/*
 * gmt_proj.c contains the specific projection functions that convert
 * longitude and latitude to x, y.  These are used via function pointers
 * set in gmt_map.c
 *
 * Map_projections include functions that will set up the transformation
 * between xy and latlon for several map projections.
 *
 * A few of the core coordinate transformation functions are based on similar
 * FORTRAN routines written by Pat Manley, Doug Shearer, and Bill Haxby, and
 * have been rewritten in C and subsequently streamlined.  The Lambert conformal
 * was originally coded up by Bernie Coakley.  The rest is coded by Wessel based
 * on P. Snyder, "Map Projections - a working manual", USGS Prof paper 1395.
 *
 * Transformations supported (both forward and inverse):
 *
 * Non-geographic projections:
 *
 *	Linear x/y[/z] scaling
 *	Polar (theta, r) scaling
 *
 * Map projections:
 *
 *  Cylindrical:
 *	Mercator
 *	Cassini Cylindrical
 *	Miller Cylindrical
 *	Oblique Mercator
 *	TM Transverse Mercator (Ellipsoidal and Spherical)
 *	UTM Universal Transverse Mercator
 *	Cylindrical Equal-area (e.g., Peters, Gall, Behrmann)
 *	Cylindrical Equidistant (Plate Carree)
 *  Azimuthal
 *	Stereographic Conformal
 *	Lambert Azimuthal Equal-Area
 *	Orthographic
 *	Azimuthal Equidistant
 *	Gnomonic
 *  Conic
 *	Albers Equal-Area Conic
 *	Lambert Conformal Conic
 *	Equidistant Conic
 *  Thematic
 *	Mollweide Equal-Area
 *	Hammer-Aitoff Equal-Area
 *	Sinusoidal Equal-Area
 *	Winkel Tripel
 *	Robinson
 *	Eckert IV
 *	Eckert IV
 *	Van der Grinten
 *
 * Author:	Paul Wessel
 * Date:	22-MAR-2006
 * Version:	4.1.2
 */
 
#define GMT_WITH_NO_PS
#include "gmt_proj.h"
#include "gmt.h"

void GMT_check_R_J(double *clon);

void GMT_check_R_J (double *clon)	/* Make sure -R and -J agree for global plots; J given priority */
{
	double lon0;

	lon0 = 0.5 * (project_info.w + project_info.e);
	if (GMT_world_map && lon0 != *clon) {
		project_info.w = *clon - 180.0;
		project_info.e = *clon + 180.0;
		if (gmtdefs.verbose) fprintf (stderr, "%s: GMT Warning: Central meridian set with -J (%g) implies -R%g/%g/%g/%g\n",
			GMT_program, *clon, project_info.w, project_info.e, project_info.s, project_info.n);
	}
	else if (!GMT_world_map) {
		lon0 = *clon - 360.0;
		while (lon0 < project_info.w) lon0 += 360.0;
		if (lon0 > project_info.e) {	/* Warn user*/
			if (gmtdefs.verbose) fprintf (stderr, "%s: GMT Warning: Central meridian outside region\n", GMT_program);
		}
	}
}

/* LINEAR TRANSFORMATIONS */

void GMT_translin (double forw, double *inv)	/* Linear forward */
{
	*inv = forw;
}

void GMT_translind (double forw, double *inv)	/* Linear forward, but with degrees*/
{
	while ((forw - project_info.central_meridian) < -180.0) forw += 360.0;
	while ((forw - project_info.central_meridian) > 180.0) forw -= 360.0;
	*inv = forw - project_info.central_meridian;
}

void GMT_itranslind (double *forw, double inv)	/* Linear inverse, but with degrees*/
{
	*forw = inv + project_info.central_meridian;
	while ((*forw - project_info.central_meridian) < -180.0) *forw += 360.0;
	while ((*forw - project_info.central_meridian) > 180.0) *forw -= 360.0;
}

void GMT_itranslin (double *forw, double inv)	/* Linear inverse */
{
	*forw = inv;
}

void GMT_translog10 (double forw, double *inv)	/* Log10 forward */
{
	*inv = d_log10 (forw);
}

void GMT_itranslog10 (double *forw, double inv) /* Log10 inverse */
{
	*forw = pow (10.0, inv);
}

void GMT_transpowx (double x, double *x_in)	/* pow x forward */
{
	*x_in = pow (x, project_info.xyz_pow[0]);
}

void GMT_itranspowx (double *x, double x_in) /* pow x inverse */
{
	*x = pow (x_in, project_info.xyz_ipow[0]);
}

void GMT_transpowy (double y, double *y_in)	/* pow y forward */
{
	*y_in = pow (y, project_info.xyz_pow[1]);
}

void GMT_itranspowy (double *y, double y_in) /* pow y inverse */
{
	*y = pow (y_in, project_info.xyz_ipow[1]);
}

void GMT_transpowz (double z, double *z_in)	/* pow z forward */
{
	*z_in = pow (z, project_info.xyz_pow[2]);
}

void GMT_itranspowz (double *z, double z_in) /* pow z inverse */
{
	*z = pow (z_in, project_info.xyz_ipow[2]);
}

/* -JP POLAR (r-theta) PROJECTION */

void GMT_vpolar (double lon0)
{
	/* Set up a Polar (theta,r) transformation */

	project_info.p_base_angle = lon0;
	project_info.central_meridian = 0.5 * (project_info.e + project_info.w);

	/* Plus pretend that it is kind of a geographic polar projection */

	project_info.north_pole = project_info.got_elevations;
	project_info.pole = (project_info.got_elevations) ? 90.0 : 0.0;
}

void GMT_polar (double x, double y, double *x_i, double *y_i)
{	/* Transform x and y to polar(cylindrical) coordinates */
	if (project_info.got_azimuths) x = 90.0 - x;		/* azimuths, not directions */
	if (project_info.got_elevations) y = 90.0 - y;		/* elevations */
	x = (x - project_info.p_base_angle) * D2R;		/* Change base line angle and convert to radians */
	sincos (x, y_i, x_i);
	(*x_i) *= y;
	(*y_i) *= y;
}

void GMT_ipolar (double *x, double *y, double x_i, double y_i)
{	/* Inversely transform both x and y from polar(cylindrical) coordinates */
	*x = R2D * d_atan2 (y_i, x_i) + project_info.p_base_angle;
	if (project_info.got_azimuths) *x = 90.0 - (*x);		/* azimuths, not directions */
	*y = hypot (x_i, y_i);
	if (project_info.got_elevations) *y = 90.0 - (*y);    /* elevations, presumably */
}

/* -JM MERCATOR PROJECTION */

void GMT_vmerc (double cmerid)
{
	/* Set up a Mercator transformation */

	if (project_info.projection != MERCATOR || !project_info.m_got_parallel) {

		/* Get here when we use -JMwidth or -Jmscale OR GMT_vmerc is called as part of -JO */

		project_info.central_meridian = cmerid;
		project_info.m_m = project_info.EQ_RAD;
		project_info.pars[1] = project_info.pars[2] = 0.0;
	}
	else {	/* Different standard parallel than equator and also set central meridian */

		/* Got here because we used -JM<clon>/<clat>/<width|scale> */

		project_info.central_meridian = project_info.pars[0];
		project_info.m_m = cosd (project_info.pars[1]) / d_sqrt (1.0 - project_info.ECC2 * sind (project_info.pars[1]) * sind (project_info.pars[1])) * project_info.EQ_RAD;	/* Put project_info.EQ_RAD here instead of in merc */
		project_info.pars[0] = project_info.pars[2];	/* Since GMT_map_init_merc expects scale/width as 0'th arg */
	}
	project_info.m_im = 1.0 / project_info.m_m;
	project_info.central_meridian_rad = project_info.central_meridian * D2R;
}

/* Mercator projection for the sphere */

void GMT_merc_sph (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Mercator x/y (project_info.EQ_RAD in project_info.m_m) */

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_latc (lat);

	*x = project_info.m_mx * lon;
	*y = (fabs (lat) < 90.0) ? project_info.m_m * d_log (tan (M_PI_4 + 0.5 * D2R * lat)) : copysign (DBL_MAX, lat);
}

void GMT_imerc_sph (double *lon, double *lat, double x, double y)
{
	/* Convert Mercator x/y to lon/lat  (project_info.EQ_RAD in project_info.m_im) */

	*lon = x * project_info.m_imx + project_info.central_meridian;
	*lat = atan (sinh (y * project_info.m_im)) * R2D;
	if (project_info.GMT_convert_latitudes) *lat = GMT_latc_to_latg (*lat);
}

/* -JY CYLINDRICAL EQUAL-AREA PROJECTION */

void GMT_vcyleq (double lon0, double slat)
{
	/* Set up a Cylindrical equal-area transformation */

	GMT_check_R_J (&lon0);
	project_info.central_meridian = lon0;
	project_info.y_rx = project_info.EQ_RAD * D2R * cosd (slat);
	project_info.y_ry = project_info.EQ_RAD / cosd (slat);
	project_info.y_i_rx = 1.0 / project_info.y_rx;
	project_info.y_i_ry = 1.0 / project_info.y_ry;
}

void GMT_cyleq (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Cylindrical equal-area x/y */

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_lata (lat);

	*x = lon * project_info.y_rx;
	*y = project_info.y_ry * sind (lat);
	if (project_info.GMT_convert_latitudes) {	/* Gotta fudge abit */
		(*x) *= project_info.Dx;
		(*y) *= project_info.Dy;
	}
}

void GMT_icyleq (double *lon, double *lat, double x, double y)
{

	/* Convert Cylindrical equal-area x/y to lon/lat */

	if (project_info.GMT_convert_latitudes) {	/* Gotta fudge abit */
		x *= project_info.iDx;
		y *= project_info.iDy;
	}
	*lon = (x * project_info.y_i_rx) + project_info.central_meridian;
	*lat = R2D * d_asin (y * project_info.y_i_ry);
	if (project_info.GMT_convert_latitudes) *lat = GMT_lata_to_latg (*lat);
}

/* -JQ CYLINDRICAL EQUIDISTANT PROJECTION */

void GMT_vcyleqdist (double lon0)
{
	/* Set up a Cylindrical equidistant transformation */

	GMT_check_R_J (&lon0);
	project_info.central_meridian = lon0;
	project_info.q_r = D2R * project_info.EQ_RAD;
	project_info.q_ir = 1.0 / project_info.q_r;
}

void GMT_cyleqdist (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Cylindrical equidistant x/y */

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	*x = lon * project_info.q_r;
	*y = lat * project_info.q_r;
}

void GMT_icyleqdist (double *lon, double *lat, double x, double y)
{

	/* Convert Cylindrical equal-area x/y to lon/lat */

	*lon = x * project_info.q_ir + project_info.central_meridian;
	*lat = y * project_info.q_ir;
}

/* -JJ MILLER CYLINDRICAL PROJECTION */

void GMT_vmiller (double lon0)
{
	/* Set up a Miller Cylindrical transformation */

	GMT_check_R_J (&lon0);
	project_info.central_meridian = lon0;
	project_info.j_x = D2R * project_info.EQ_RAD;
	project_info.j_y = 1.25 * project_info.EQ_RAD;
	project_info.j_ix = 1.0 / project_info.j_x; 
	project_info.j_iy = 1.0 / project_info.j_y; 
}

void GMT_miller (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Miller Cylindrical x/y */

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	*x = lon * project_info.j_x;
	*y = project_info.j_y * d_log (tan (M_PI_4 + 0.4 * lat * D2R));
}

void GMT_imiller (double *lon, double *lat, double x, double y)
{

	/* Convert Miller Cylindrical x/y to lon/lat */

	*lon = x * project_info.j_ix + project_info.central_meridian;
	*lat = 2.5 * R2D * atan (exp (y * project_info.j_iy)) - 112.5;
}

/* -JS POLAR STEREOGRAPHIC PROJECTION */

void GMT_vstereo (double rlong0, double plat)
{
	/* Set up a Stereographic transformation */

	double clat;
	/* GMT_check_R_J (&rlong0); */
	if (project_info.GMT_convert_latitudes) {	/* Set Conformal radius and pole latitude */
		GMT_scale_eqrad ();
		clat = GMT_latg_to_latc (plat);
	}
	else
		clat = plat;

	project_info.central_meridian = rlong0;
	project_info.pole = plat;		/* This is always geodetic */
	project_info.sinp = sind (clat);	/* These may be conformal */
	project_info.cosp = cosd (clat);
	project_info.north_pole = (plat > 0.0);
	project_info.s_c = 2.0 * project_info.EQ_RAD * gmtdefs.map_scale_factor;
	project_info.s_ic = 1.0 / project_info.s_c;
}

void GMT_plrs_sph (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to x/y using Spherical polar projection */
	double rho, slon, clon;

	if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_latc (lat);
	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;

	lon *= D2R;
	sincos (lon, &slon, &clon);
	if (project_info.north_pole) {
		rho = project_info.s_c * tan (M_PI_4 - 0.5 * D2R * lat);
		*y = -rho * clon;
		*x =  rho * slon;
	}
	else {
		rho = project_info.s_c * tan (M_PI_4 + 0.5 * D2R * lat);
		*y = rho * clon;
		*x = rho * slon;
	}
	if (project_info.GMT_convert_latitudes) {	/* Gotta fudge abit */
		(*x) *= project_info.Dx;
		(*y) *= project_info.Dy;
	}
}

void GMT_iplrs_sph (double *lon, double *lat, double x, double y)
{
	double c;

	/* Convert Spherical polar x/y to lon/lat */

	if (x == 0.0 && y == 0.0) {
		*lon = project_info.central_meridian;
		*lat = project_info.pole;
		return;
	}

	if (project_info.GMT_convert_latitudes) {	/* Undo effect of fudge factors */
		x *= project_info.iDx;
		y *= project_info.iDy;
	}
	c = 2.0 * atan (hypot (x, y) * project_info.s_ic);

	if (project_info.north_pole) {
		*lon = project_info.central_meridian + d_atan2 (x, -y) * R2D;
		*lat = d_asin (cos (c)) * R2D;
	}
	else {
		*lon = project_info.central_meridian + d_atan2 (x, y) * R2D;
		*lat = d_asin (-cos (c)) * R2D;
	}
	if (project_info.GMT_convert_latitudes) *lat = GMT_latc_to_latg (*lat);

}

void GMT_stereo1_sph (double lon, double lat, double *x, double *y)
{
	double dlon, sin_dlon, cos_dlon, s, c, cc, A;

	/* Convert lon/lat to x/y using Spherical stereographic projection, oblique view */

	if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_latc (lat);
	dlon = D2R * (lon - project_info.central_meridian);
	lat *= D2R;
	sincos (dlon, &sin_dlon, &cos_dlon);
	sincos (lat, &s, &c);
	cc = c * cos_dlon;
	A = project_info.s_c / (1.0 + project_info.sinp * s + project_info.cosp * cc);
	*x = A * c * sin_dlon;
	*y = A * (project_info.cosp * s - project_info.sinp * cc);
	if (project_info.GMT_convert_latitudes) {	/* Gotta fudge abit */
		(*x) *= project_info.Dx;
		(*y) *= project_info.Dy;
	}
}

void GMT_istereo_sph (double *lon, double *lat, double x, double y)
{
	double rho, c, sin_c, cos_c;

	if (x == 0.0 && y == 0.0) {
		*lon = project_info.central_meridian;
		*lat = project_info.pole;
	}
	else {
		if (project_info.GMT_convert_latitudes) {	/* Undo effect of fudge factors */
			x *= project_info.iDx;
			y *= project_info.iDy;
		}
		rho = hypot (x, y);
		c = 2.0 * atan (rho * project_info.s_ic);
		sincos (c, &sin_c, &cos_c);
		*lat = d_asin (cos_c * project_info.sinp + (y * sin_c * project_info.cosp / rho)) * R2D;
		*lon = R2D * atan (x * sin_c / (rho * project_info.cosp * cos_c - y * project_info.sinp * sin_c))
		       + project_info.central_meridian;
		if (project_info.GMT_convert_latitudes) *lat = GMT_latc_to_latg (*lat);
	}
}

/* Spherical equatorial view */

void GMT_stereo2_sph (double lon, double lat, double *x, double *y)
{
	double dlon, s, c, clon, slon, A;

	/* Convert lon/lat to x/y using stereographic projection, equatorial view */

	dlon = lon - project_info.central_meridian;
	if (fabs (dlon - 180.0) < GMT_CONV_LIMIT) {
		*x = *y = 0.0;
	}
	else {
		if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_latc (lat);
		dlon *= D2R;
		lat *= D2R;
		sincos (lat, &s, &c);
		sincos (dlon, &slon, &clon);
		A = project_info.s_c / (1.0 + c * clon);
		*x = A * c * slon;
		*y = A * s;
		if (project_info.GMT_convert_latitudes) {	/* Gotta fudge abit */
			(*x) *= project_info.Dx;
			(*y) *= project_info.Dy;
		}
	}
}

/* -JL LAMBERT CONFORMAL CONIC PROJECTION */

void GMT_vlamb (double rlong0, double rlat0, double pha, double phb)
{
	/* Set up a Lambert Conformal Conic projection (Spherical when e = 0) */

	double t_pha, m_pha, t_phb, m_phb, t_rlat0;

	GMT_check_R_J (&rlong0);
	project_info.north_pole = (rlat0 > 0.0);
	project_info.pole = (project_info.north_pole) ? 90.0 : -90.0;
	pha *= D2R;
	phb *= D2R;

	t_pha = tan (M_PI_4 - 0.5 * pha) / pow ((1.0 - project_info.ECC * 
		sin (pha)) / (1.0 + project_info.ECC * sin (pha)), project_info.half_ECC);
	m_pha = cos (pha) / d_sqrt (1.0 - project_info.ECC2 
		* pow (sin (pha), 2.0));
	t_phb = tan (M_PI_4 - 0.5 * phb) / pow ((1.0 - project_info.ECC *
		sin (phb)) / (1.0 + project_info.ECC * sin (phb)), project_info.half_ECC);
	m_phb = cos (phb) / d_sqrt (1.0 - project_info.ECC2 
		* pow (sin (phb), 2.0));
	t_rlat0 = tan (M_PI_4 - 0.5 * rlat0 * D2R) /
		pow ((1.0 - project_info.ECC * sin (rlat0 * D2R)) /
		(1.0 + project_info.ECC * sin (rlat0 * D2R)), project_info.half_ECC);

	if(pha != phb)
		project_info.l_N = (d_log(m_pha) - d_log(m_phb))/(d_log(t_pha) - d_log(t_phb));
	else
		project_info.l_N = sin(pha);

	project_info.l_i_N = 1.0 / project_info.l_N;
	project_info.l_F = m_pha / (project_info.l_N * pow (t_pha, project_info.l_N));
	project_info.central_meridian = rlong0;
	project_info.l_rF = project_info.EQ_RAD * project_info.l_F;
	project_info.l_i_rF = 1.0 / project_info.l_rF;
	project_info.l_rho0 = project_info.l_rF * pow (t_rlat0, project_info.l_N);
	project_info.l_Nr = project_info.l_N * D2R;
	project_info.l_i_Nr = 1.0 / project_info.l_Nr;
}

void GMT_lamb (double lon, double lat, double *x, double *y)
{
	double rho, theta, hold1, hold2, hold3, es, s, c;

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	lat *= D2R;

	es = project_info.ECC * sin (lat);
	hold2 = pow (((1.0 - es) / (1.0 + es)), project_info.half_ECC);
	hold3 = tan (M_PI_4 - 0.5 * lat);
	if (fabs (hold3) < GMT_CONV_LIMIT)
		hold1 = 0.0;
	else
		hold1 = pow (hold3 / hold2, project_info.l_N);
	rho = project_info.l_rF * hold1;
	theta = project_info.l_Nr * lon;
	sincos (theta, &s, &c);
	*x = rho * s;
	*y = project_info.l_rho0 - rho * c;
}

void GMT_ilamb (double *lon, double *lat, double x, double y)
{
	int i;
	double theta, rho, t, tphi, phi, delta, dy, r;

	theta = atan (x / (project_info.l_rho0 - y));
	*lon = theta * project_info.l_i_Nr + project_info.central_meridian;

	dy = project_info.l_rho0 - y;
	rho = copysign (hypot (x, dy), project_info.l_N);
	t = pow ((rho * project_info.l_i_rF), project_info.l_i_N);
	phi = tphi = M_PI_2 - 2.0 * atan (t);
	delta = 1.0;
	for (i = 0; i < 100 && delta > GMT_CONV_LIMIT; i++) {
		r = project_info.ECC * sin (tphi);
		phi = M_PI_2 - 2.0 * atan (t * pow ((1.0 - r) / (1.0 + r), project_info.half_ECC));
		delta = fabs (fabs (tphi) - fabs (phi));
		tphi = phi;
	}
	*lat = phi * R2D;
}

/* Spherical cases of Lambert */

void GMT_lamb_sph (double lon, double lat, double *x, double *y)
{
	double rho, theta, A, t, s, c;

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_latc (lat);

	lat *= D2R;
	t = tan (M_PI_4 - 0.5 * lat);
	A = (fabs (t) < GMT_CONV_LIMIT) ? 0.0 : pow (t, project_info.l_N);
	rho = project_info.l_rF * A;
	theta = project_info.l_Nr * lon;

	sincos (theta, &s, &c);
	*x = rho * s;
	*y = project_info.l_rho0 - rho * c;
}

void GMT_ilamb_sph (double *lon, double *lat, double x, double y)
{
	double theta, rho, t;

	theta = atan (x / (project_info.l_rho0 - y));
	rho = hypot (x, project_info.l_rho0 - y);
	if (project_info.l_N < 0.0) rho = -rho;
	t = pow ((rho * project_info.l_i_rF), -project_info.l_i_N);

	*lon = theta * project_info.l_i_Nr + project_info.central_meridian;
	*lat = 2.0 * R2D * atan (t) - 90.0;
	if (project_info.GMT_convert_latitudes) *lat = GMT_latc_to_latg (*lat);
}

/* -JO OBLIQUE MERCATOR PROJECTION */

void GMT_oblmrc (double lon, double lat, double *x, double *y)
{
	/* Convert a longitude/latitude point to Oblique Mercator coordinates 
	 * by way of rotation coordinates and then using regular Mercator */
	double tlon, tlat;

	GMT_obl (lon * D2R, lat * D2R, &tlon, &tlat);

	*x = project_info.m_m * tlon;
	*y = (fabs (tlat) < M_PI_2) ? project_info.m_m * d_log (tan (M_PI_4 + 0.5 * tlat)) : copysign (DBL_MAX, tlat);
}

void GMT_ioblmrc (double *lon, double *lat, double x, double y)
{
	/* Convert a longitude/latitude point from Oblique Mercator coordinates 
	 * by way of regular Mercator and then rotate coordinates */
	double tlon, tlat;

	tlon = x * project_info.m_im;
	tlat = atan (sinh (y * project_info.m_im));

	GMT_iobl (lon, lat, tlon, tlat);

	(*lon) *= R2D;
	(*lat) *= R2D;
}

void GMT_obl (double lon, double lat, double *olon, double *olat)
{
	/* Convert a longitude/latitude point to Oblique lon/lat (all in rads) */
	double p_cross_x[3], X[3];

	GMT_geo_to_cart (&lat, &lon, X, FALSE);

	*olat = d_asin (GMT_dot3v (X, project_info.o_FP));

	GMT_cross3v (project_info.o_FP, X, p_cross_x);
	GMT_normalize3v (p_cross_x);

	*olon = copysign (d_acos (GMT_dot3v (p_cross_x, project_info.o_FC)), GMT_dot3v (X, project_info.o_FC));
}

void GMT_iobl (double *lon, double *lat, double olon, double olat)
{
	/* Convert a longitude/latitude point from Oblique lon/lat  (all in rads) */
	double p_cross_x[3], X[3];

	GMT_geo_to_cart (&olat, &olon, X, FALSE);
	*lat = d_asin (GMT_dot3v (X, project_info.o_IP));

	GMT_cross3v (project_info.o_IP, X, p_cross_x);
	GMT_normalize3v (p_cross_x);

	*lon = copysign (d_acos (GMT_dot3v (p_cross_x, project_info.o_IC)), GMT_dot3v (X, project_info.o_IC));

	while ((*lon) < 0.0) (*lon) += TWO_PI;
	while ((*lon) >= TWO_PI) (*lon) -= TWO_PI;
}

/* -JT TRANSVERSE MERCATOR PROJECTION */

void GMT_vtm (double lon0, double lat0)
{
	/* Set up an TM projection */
	double e1, lat2, s2, c2;

	/* GMT_check_R_J (&lon0); */
	e1 = (1.0 - d_sqrt (project_info.one_m_ECC2)) / (1.0 + d_sqrt (project_info.one_m_ECC2));
	project_info.t_e2 = project_info.ECC2 * project_info.i_one_m_ECC2;
	project_info.t_c1 = 1.0 - (1.0/4.0) * project_info.ECC2 - (3.0/64.0) * project_info.ECC4 - (5.0/256.0) * project_info.ECC6;
	project_info.t_c2 = -((3.0/8.0) * project_info.ECC2 + (3.0/32.0) * project_info.ECC4 + (25.0/768.0) * project_info.ECC6);
	project_info.t_c3 = (15.0/128.0) * project_info.ECC4 + (45.0/512.0) * project_info.ECC6;
	project_info.t_c4 = -(35.0/768.0) * project_info.ECC6;
	project_info.t_i1 = 1.0 / (project_info.EQ_RAD * project_info.t_c1);
	project_info.t_i2 = (3.0/2.0) * e1 - (29.0/12.0) * pow (e1, 3.0);
	project_info.t_i3 = (21.0/8.0) * e1 * e1 - (1537.0/128.0) * pow (e1, 4.0);
	project_info.t_i4 = (151.0/24.0) * pow (e1, 3.0);
	project_info.t_i5 = (1097.0/64.0) * pow (e1, 4.0);
	project_info.central_meridian = lon0;
	lat0 *= D2R;
	project_info.t_lat0 = lat0;	/* In radians */
	lat2 = 2.0 * lat0;
	sincos (lat2, &s2, &c2);
	project_info.t_M0 = project_info.EQ_RAD * (project_info.t_c1 * lat0 + s2 * (project_info.t_c2 + c2 * (project_info.t_c3 + c2 * project_info.t_c4)));
	project_info.t_r = project_info.EQ_RAD * gmtdefs.map_scale_factor;
	project_info.t_ir = 1.0 / project_info.t_r;
}

/* Ellipsoidal TM functions */

void GMT_tm (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to TM x/y */
	double N, T, T2, C, A, M, dlon, tan_lat, A2, A3, A5, lat2, s, c, s2, c2;

	if (fabs (fabs (lat) - 90.0) < GMT_CONV_LIMIT) {
		M = project_info.EQ_RAD * project_info.t_c1 * M_PI_2;
		*x = 0.0;
		*y = gmtdefs.map_scale_factor * M;
	}
	else {
		lat *= D2R;
		lat2 = 2.0 * lat;
		sincos (lat, &s, &c);
		sincos (lat2, &s2, &c2);
		tan_lat = s / c;
		M = project_info.EQ_RAD * (project_info.t_c1 * lat + s2 * (project_info.t_c2 + c2 * (project_info.t_c3 + c2 * project_info.t_c4)));
		dlon = lon - project_info.central_meridian;
		if (fabs (dlon) > 360.0) dlon += copysign (360.0, -dlon);
		if (fabs (dlon) > 180.0) dlon = copysign (360.0 - fabs (dlon), -dlon);
		N = project_info.EQ_RAD / d_sqrt (1.0 - project_info.ECC2 * s * s);
		T = tan_lat * tan_lat;
		T2 = T * T;
		C = project_info.t_e2 * c * c;
		A = dlon * D2R * c;
		A2 = A * A;	A3 = A2 * A;	A5 = A3 * A2;
		*x = gmtdefs.map_scale_factor * N * (A + (1.0 - T + C) * (A3 * 0.16666666666666666667)
			+ (5.0 - 18.0 * T + T2 + 72.0 * C - 58.0 * project_info.t_e2) * (A5 * 0.00833333333333333333));
		A3 *= A;	A5 *= A;
		*y = gmtdefs.map_scale_factor * (M - project_info.t_M0 + N * tan_lat * (0.5 * A2 + (5.0 - T + 9.0 * C + 4.0 * C * C) * (A3 * 0.04166666666666666667)
			+ (61.0 - 58.0 * T + T2 + 600.0 * C - 330.0 * project_info.t_e2) * (A5 * 0.00138888888888888889)));
	}
}

void GMT_itm (double *lon, double *lat, double x, double y)
{
	/* Convert TM x/y to lon/lat */
	double M, mu, u2, s, c, phi1, C1, C12, T1, T12, tmp, tmp2, N1, R_1, D, D2, D3, D5, tan_phi1, cp2;

	M = y / gmtdefs.map_scale_factor + project_info.t_M0;
	mu = M * project_info.t_i1;

	u2 = 2.0 * mu;
	sincos (u2, &s, &c);
	phi1 = mu + s * (project_info.t_i2 + c * (project_info.t_i3 + c * (project_info.t_i4 + c * project_info.t_i5)));

	sincos (phi1, &s, &c);
	tan_phi1 = s / c;
	cp2 = c * c;
	C1 = project_info.t_e2 * cp2;
	C12 = C1 * C1;
	T1 = tan_phi1 * tan_phi1;
	T12 = T1 * T1;
	tmp = 1.0 - project_info.ECC2 * (1.0 - cp2);
	tmp2 = d_sqrt (tmp);
	N1 = project_info.EQ_RAD / tmp2;
	R_1 = project_info.EQ_RAD * project_info.one_m_ECC2 / (tmp * tmp2);
	D = x / (N1 * gmtdefs.map_scale_factor);
	D2 = D * D;	D3 = D2 * D;	D5 = D3 * D2;

	*lon = project_info.central_meridian + R2D * (D - (1.0 + 2.0 * T1 + C1) * (D3 * 0.16666666666666666667) 
		+ (5.0 - 2.0 * C1 + 28.0 * T1 - 3.0 * C12 + 8.0 * project_info.t_e2 + 24.0 * T12)
		* (D5 * 0.00833333333333333333)) / c;
	D3 *= D;	D5 *= D;
	*lat = phi1 - (N1 * tan_phi1 / R_1) * (0.5 * D2 -
		(5.0 + 3.0 * T1 + 10.0 * C1 - 4.0 * C12 - 9.0 * project_info.t_e2) * (D3 * 0.04166666666666666667)
		+ (61.0 + 90.0 * T1 + 298 * C1 + 45.0 * T12 - 252.0 * project_info.t_e2 - 3.0 * C12) * (D5 * 0.00138888888888888889));
	(*lat) *= R2D;
}

/*Spherical TM functions */

void GMT_tm_sph (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to TM x/y by spherical formula */
	double dlon, b, clat, slat, clon, slon, xx, yy;

	dlon = lon - project_info.central_meridian;
	if (fabs (dlon) > 360.0) dlon += copysign (360.0, -dlon);
	if (fabs (dlon) > 180.0) dlon = copysign (360.0 - fabs (dlon), -dlon);

	if (fabs (lat) > 90.0) {
		/* Invalid latitude.  Treat as in GMT_merc_sph(), but transversely:  */
		*x = copysign (1.0e100, dlon);
		*y = 0.0;
		return;
	}

	if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_latc (lat);

	lat *= D2R;
	dlon *= D2R;
	sincos (lat, &slat, &clat);
	sincos (dlon, &slon, &clon);
	b = clat * slon;
	if (fabs(b) >= 1.0) {
		/* This corresponds to the transverse "pole"; the point at x = +-infinity, y = -lat0.  
			Treat as in GMT_merc_sph(), but transversely:  */
		*x = copysign (1.0e100, dlon);
		*y = -project_info.t_r * project_info.t_lat0;
		return;
	}

	xx = atanh (b);

	/* this should get us "over the pole";
	   see not Snyder's formula but his example Fig. 10 on p. 50:  */

	yy = atan2 (slat, (clat * clon)) - project_info.t_lat0;
	if (yy < -M_PI_2) yy += TWO_PI;
	*x = project_info.t_r * xx;
	*y = project_info.t_r * yy;
}

void GMT_itm_sph (double *lon, double *lat, double x, double y)
{
	/* Convert TM x/y to lon/lat by spherical approximation.  */

	double xx, yy, sinhxx, coshxx, sind, cosd, lambda, phi;

	xx = x * project_info.t_ir;
	yy = y * project_info.t_ir + project_info.t_lat0;

	sinhxx = sinh (xx);
	coshxx = cosh (xx);

	sincos (yy, &sind, &cosd);
	phi = R2D * asin (sind / coshxx);
	*lat = phi;

	lambda = R2D * atan2 (sinhxx, cosd);
	*lon = lambda + project_info.central_meridian;
	if (project_info.GMT_convert_latitudes) *lat = GMT_latc_to_latg (*lat);
}

/* -JU UNIVERSAL TRANSVERSE MERCATOR PROJECTION */

/* Ellipsoidal UTM */

void GMT_utm (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to UTM x/y */

	if (lon < 0.0) lon += 360.0;
	GMT_tm (lon, lat, x, y);
	(*x) += FALSE_EASTING;
	if (!project_info.north_pole) (*y) += FALSE_NORTHING;	/* For S hemisphere, add 10^7 m */
}

void GMT_iutm (double *lon, double *lat, double x, double y)
{
	/* Convert UTM x/y to lon/lat */

	x -= FALSE_EASTING;
	if (!project_info.north_pole) y -= FALSE_NORTHING;
	GMT_itm (lon, lat, x, y);
}

/* Spherical UTM */

void GMT_utm_sph (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to UTM x/y */

	if (lon < 0.0) lon += 360.0;
	GMT_tm_sph (lon, lat, x, y);
	(*x) += FALSE_EASTING;
	if (!project_info.north_pole) (*y) += FALSE_NORTHING;	/* For S hemisphere, add 10^7 m */
}

void GMT_iutm_sph (double *lon, double *lat, double x, double y)
{
	/* Convert UTM x/y to lon/lat */

	x -= FALSE_EASTING;
	if (!project_info.north_pole) y -= FALSE_NORTHING;
	GMT_itm_sph (lon, lat, x, y);
}

/* -JA LAMBERT AZIMUTHAL EQUAL AREA PROJECTION */

void GMT_vlambeq (double lon0, double lat0)
{
	/* Set up Spherical Lambert Azimuthal Equal-Area projection */

	/* GMT_check_R_J (&lon0); */
	project_info.central_meridian = lon0;
	project_info.pole = lat0;
	project_info.sinp = sind (lat0);
	project_info.cosp = cosd (lat0);
}

void GMT_lambeq (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Spherical Lambert Azimuthal Equal-Area x/y */
	double k, tmp, sin_lat, cos_lat, sin_lon, cos_lon, c;

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_lata (lat);
	lon *= D2R;
	lat *= D2R;
	sincos (lat, &sin_lat, &cos_lat);
	sincos (lon, &sin_lon, &cos_lon);
	c = cos_lat * cos_lon;
	tmp = 1.0 + project_info.sinp * sin_lat + project_info.cosp * c;
	if (tmp > 0.0) {
		k = project_info.EQ_RAD * d_sqrt (2.0 / tmp);
		*x = k * cos_lat * sin_lon;
		*y = k * (project_info.cosp * sin_lat - project_info.sinp * c);
		if (project_info.GMT_convert_latitudes) {	/* Gotta fudge abit */
			(*x) *= project_info.Dx;
			(*y) *= project_info.Dy;
		}
	}
	else
		*x = *y = -DBL_MAX;
}

void GMT_ilambeq (double *lon, double *lat, double x, double y)
{
	/* Convert Lambert Azimuthal Equal-Area x/yto lon/lat */
	double rho, c, sin_c, cos_c;

	if (project_info.GMT_convert_latitudes) {	/* Undo effect of fudge factors */
		x *= project_info.iDx;
		y *= project_info.iDy;
	}

	rho = hypot (x, y);

	if (fabs (rho) < GMT_CONV_LIMIT) {
		*lat = project_info.pole;
		*lon = project_info.central_meridian;
	}
	else {
		c = 2.0 * d_asin (0.5 * rho * project_info.i_EQ_RAD);
		sincos (c, &sin_c, &cos_c);
		*lat = d_asin (cos_c * project_info.sinp + (y * sin_c * project_info.cosp / rho)) * R2D;
		if (project_info.n_polar)
			*lon = project_info.central_meridian + R2D * d_atan2 (x, -y);
		else if (project_info.s_polar)
			*lon = project_info.central_meridian + R2D * d_atan2 (x, y);
		else
			*lon = project_info.central_meridian +
				R2D * d_atan2 (x * sin_c, (rho * project_info.cosp * cos_c - y * project_info.sinp * sin_c));
		if (project_info.GMT_convert_latitudes) *lat = GMT_lata_to_latg (*lat);
	}
}

/* -JG ORTHOGRAPHIC PROJECTION */

void GMT_vortho (double lon0, double lat0)
{
	/* Set up Orthographic projection */

	/* GMT_check_R_J (&lon0); */
	project_info.central_meridian = lon0;
	project_info.pole = lat0;
	project_info.sinp = sin (lat0 * D2R);
	project_info.cosp = cos (lat0 * D2R);
}

void GMT_ortho (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Orthographic x/y */
	double sin_lat, cos_lat, sin_lon, cos_lon;

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	lon *= D2R;
	lat *= D2R;

	sincos (lat, &sin_lat, &cos_lat);
	sincos (lon, &sin_lon, &cos_lon);
	*x = project_info.EQ_RAD * cos_lat * sin_lon;
	*y = project_info.EQ_RAD * (project_info.cosp * sin_lat - project_info.sinp * cos_lat * cos_lon);
}

void GMT_iortho (double *lon, double *lat, double x, double y)
{
	/* Convert Orthographic x/yto lon/lat */
	double rho, sin_c, cos_c;

	rho = hypot (x, y);

	if (fabs (rho) < GMT_CONV_LIMIT) {
		*lat = project_info.pole;
		*lon = project_info.central_meridian;
	}
	else {
		sin_c = rho * project_info.i_EQ_RAD;
		cos_c = d_sqrt (1.0 - sin_c * sin_c);
		*lat = d_asin (cos_c * project_info.sinp + (y * sin_c * project_info.cosp / rho)) * R2D;
		if (project_info.n_polar)
			*lon = project_info.central_meridian + R2D * d_atan2 (x, -y);
		else if (project_info.s_polar)
			*lon = project_info.central_meridian + R2D * d_atan2 (x, y);
		else
			*lon = project_info.central_meridian +
				R2D * d_atan2 (x * sin_c, (rho * project_info.cosp * cos_c - y * project_info.sinp * sin_c));
	}
}

/* -JF GNOMONIC PROJECTION */

void GMT_vgnomonic (double lon0, double lat0, double horizon)
{
	/* Set up Gnomonic projection */

	/* GMT_check_R_J (&lon0); */
	project_info.central_meridian = lon0;
	project_info.f_horizon = horizon;
	project_info.pole = lat0;
	project_info.north_pole = (lat0 > 0.0);
	lat0 *= D2R;
	project_info.sinp = sin (lat0);
	project_info.cosp = cos (lat0);
}

void GMT_gnomonic (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Gnomonic x/y */
	double k, sin_lat, cos_lat, sin_lon, cos_lon, cc;

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	lon *= D2R;
	lat *= D2R;
	sincos (lat, &sin_lat, &cos_lat);
	sincos (lon, &sin_lon, &cos_lon);

	cc = cos_lat * cos_lon;
	k =  project_info.EQ_RAD / (project_info.sinp * sin_lat + project_info.cosp * cc);
	*x = k * cos_lat * sin_lon;
	*y = k * (project_info.cosp * sin_lat - project_info.sinp * cc);
}

void GMT_ignomonic (double *lon, double *lat, double x, double y)
{
	/* Convert Gnomonic x/y to lon/lat */
	double rho, c, sin_c, cos_c;

	rho = hypot (x, y);

	if (fabs (rho) < GMT_CONV_LIMIT) {
		*lat = project_info.pole;
		*lon = project_info.central_meridian;
	}
	else {
		c = atan (rho * project_info.i_EQ_RAD);
		sincos (c, &sin_c, &cos_c);
		*lat = d_asin (cos_c * project_info.sinp + (y * sin_c * project_info.cosp / rho)) * R2D;
		if (project_info.polar && project_info.north_pole)
			*lon = project_info.central_meridian + R2D * d_atan2 (x, -y);
		else if (project_info.polar)
			*lon = project_info.central_meridian + R2D * d_atan2 (x, y);
		else
			*lon = project_info.central_meridian +
				R2D * d_atan2 (x * sin_c, (rho * project_info.cosp * cos_c - y * project_info.sinp * sin_c));
	}
}

/* -JE AZIMUTHAL EQUIDISTANT PROJECTION */

void GMT_vazeqdist (double lon0, double lat0)
{
	/* Set up azimuthal equidistant projection */

	/* GMT_check_R_J (&lon0); */
	project_info.central_meridian = lon0;
	project_info.pole = lat0;
	project_info.sinp = sin (lat0 * D2R);
	project_info.cosp = cos (lat0 * D2R);
}

void GMT_azeqdist (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to azimuthal equidistant x/y */
	double k, cc, c, clat, slon, clon, slat, t;

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	lon *= D2R;
	lat *= D2R;
	sincos (lat, &slat, &clat);
	sincos (lon, &slon, &clon);

	t = clat * clon;
	cc = project_info.sinp * slat + project_info.cosp * t;
	if (fabs (cc) >= 1.0)
		*x = *y = 0.0;
	else {
		c = d_acos (cc);
		k = project_info.EQ_RAD * c / sin (c);
		*x = k * clat * slon;
		*y = k * (project_info.cosp * slat - project_info.sinp * t);
	}
}

void GMT_iazeqdist (double *lon, double *lat, double x, double y)
{
	/* Convert azimuthal equidistant x/yto lon/lat */
	double rho, c, sin_c, cos_c;

	rho = hypot (x, y);

	if (fabs (rho) < GMT_CONV_LIMIT) {
		*lat = project_info.pole;
		*lon = project_info.central_meridian;
	}
	else {
		c = rho * project_info.i_EQ_RAD;
		sincos (c, &sin_c, &cos_c);
		*lat = d_asin (cos_c * project_info.sinp + (y * sin_c * project_info.cosp / rho)) * R2D;
		if (project_info.n_polar)
			*lon = project_info.central_meridian + R2D * d_atan2 (x, -y);
		else if (project_info.s_polar)
			*lon = project_info.central_meridian + R2D * d_atan2 (x, y);
		else
			*lon = project_info.central_meridian +
				R2D * d_atan2 (x * sin_c, (rho * project_info.cosp * cos_c - y * project_info.sinp * sin_c));
		if ((*lon) <= -180) (*lon) += 360.0;
	}
}

/* -JW MOLLWEIDE EQUAL AREA PROJECTION */

void GMT_vmollweide (double lon0, double scale)
{
	/* Set up Mollweide Equal-Area projection */

	GMT_check_R_J (&lon0);
	project_info.central_meridian = lon0;
	project_info.w_x = project_info.EQ_RAD * D2R * d_sqrt (8.0) / M_PI;
	project_info.w_y = project_info.EQ_RAD * M_SQRT2;
	project_info.w_iy = 1.0 / project_info.w_y;
	project_info.w_r = 0.25 * (scale * project_info.M_PR_DEG * 360.0);	/* = Half the minor axis */
}

void GMT_mollweide (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Mollweide Equal-Area x/y */
	int i;
	double phi, delta, psin_lat, c, s;

	if (fabs (fabs (lat) - 90.0) < GMT_CONV_LIMIT) {	/* Special case */
		*x = 0.0;
		*y = copysign (project_info.w_y, lat);
		return;
	}

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_lata (lat);
	lat *= D2R;

	phi = lat;
	psin_lat = M_PI * sin (lat);
	i = 0;
	do {
		i++;
		sincos (phi, &s, &c);
		delta = -(phi + s - psin_lat) / (1.0 + c);
		phi += delta;
	}
	while (fabs (delta) > GMT_CONV_LIMIT && i < 100);
	phi *= 0.5;
	sincos (phi, &s, &c);
	*x = project_info.w_x * lon * c;
	*y = project_info.w_y * s;
}

void GMT_imollweide (double *lon, double *lat, double x, double y)
{
	/* Convert Mollweide Equal-Area x/y to lon/lat */
	double phi, phi2;

	phi = d_asin (y * project_info.w_iy);
	phi2 = 2.0 * phi;
	*lat = R2D * d_asin ((phi2 + sin (phi2)) / M_PI);
	*lon = project_info.central_meridian + (x / (project_info.w_x * cos (phi)));
	if (project_info.GMT_convert_latitudes) *lat = GMT_lata_to_latg (*lat);
}

/* -JH HAMMER-AITOFF EQUAL AREA PROJECTION */

void GMT_vhammer (double lon0, double scale)
{
	/* Set up Hammer-Aitoff Equal-Area projection */

	GMT_check_R_J (&lon0);
	project_info.central_meridian = lon0;
	project_info.w_r = 0.25 * (scale * project_info.M_PR_DEG * 360.0);	/* = Half the minor axis */
}

void GMT_hammer (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Hammer-Aitoff Equal-Area x/y */
	double slat, clat, slon, clon, D;

	if (fabs (fabs (lat) - 90.0) < GMT_CONV_LIMIT) {	/* Save time */
		*x = 0.0;
		*y = M_SQRT2 * copysign (project_info.EQ_RAD, lat);
		return;
	}

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_lata (lat);
	lat *= D2R;
	lon *= (0.5 * D2R);
	sincos (lat, &slat, &clat);
	sincos (lon, &slon, &clon);

	D = project_info.EQ_RAD * sqrt (2.0 / (1.0 + clat * clon));
	*x = 2.0 * D * clat * slon;
	*y = D * slat;
}

void GMT_ihammer (double *lon, double *lat, double x, double y)
{
	/* Convert Hammer_Aitoff Equal-Area x/y to lon/lat */
	double rho, c, angle;

	x *= 0.5;
	rho = hypot (x, y);

	if (fabs (rho) < GMT_CONV_LIMIT) {
		*lat = 0.0;
		*lon = project_info.central_meridian;
	}
	else {
		c = 2.0 * d_asin (0.5 * rho * project_info.i_EQ_RAD);
		*lat = d_asin (y * sin (c) / rho) * R2D;
		if (fabs (c - M_PI_2) < GMT_CONV_LIMIT)
			angle = (fabs (x) < GMT_CONV_LIMIT) ? 0.0 : copysign (180.0, x);
		else
			angle = 2.0 * R2D * atan (x * tan (c) / rho);
		*lon = project_info.central_meridian + angle;
		if (project_info.GMT_convert_latitudes) *lat = GMT_lata_to_latg (*lat);
	}
}

/* -JV VAN DER GRINTEN PROJECTION */

void GMT_vgrinten (double lon0, double scale)
{
	/* Set up van der Grinten projection */

	GMT_check_R_J (&lon0);
	project_info.central_meridian = lon0;
	project_info.v_r = M_PI * project_info.EQ_RAD;
	project_info.v_ir = 1.0 / project_info.v_r;
}

void GMT_grinten (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to van der Grinten x/y */
	double flat, A, A2, G, P, P2, Q, P2A2, i_P2A2, GP2, c, s, theta;

	flat = fabs (lat);
	if (flat > (90.0 - GMT_CONV_LIMIT)) {	/* Save time */
		*x = 0.0;
		*y = M_PI * copysign (project_info.EQ_RAD, lat);
		return;
	}
	if (fabs (lon - project_info.central_meridian) < GMT_CONV_LIMIT) {	/* Save time */
		theta = d_asin (2.0 * fabs (lat) / 180.0);
		*x = 0.0;
		*y = M_PI * copysign (project_info.EQ_RAD, lat) * tan (0.5 * theta);
		return;
	}

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;

	if (flat < GMT_CONV_LIMIT) {	/* Save time */
		*x = project_info.EQ_RAD * D2R * lon;
		*y = 0.0;
		return;
	}

	theta = d_asin (2.0 * fabs (lat) / 180.0);

	A = 0.5 * fabs (180.0 / lon - lon / 180.0);
	A2 = A * A;
	sincos (theta, &s, &c);
	G = c / (s + c - 1.0);
	P = G * (2.0 / s - 1.0);
	Q = A2 + G;
	P2 = P * P;
	P2A2 = P2 + A2;
	GP2 = G - P2;
	i_P2A2 = 1.0 / P2A2;

	*x = copysign (project_info.v_r, lon) * (A * GP2 + sqrt (A2 * GP2 * GP2 - P2A2 * (G*G - P2))) * i_P2A2;
	*y = copysign (project_info.v_r, lat) * (P * Q - A * sqrt ((A2 + 1.0) * P2A2 - Q * Q)) * i_P2A2;
}

void GMT_igrinten (double *lon, double *lat, double x, double y)
{
	/* Convert van der Grinten x/y to lon/lat */
	double x2, c1, x2y2, y2, c2, c3, d, a1, m1, theta1, p;

	x *= project_info.v_ir;
	y *= project_info.v_ir;
	x2 = x * x;	y2 = y * y;
	x2y2 = x2 + y2;
	c1 = -fabs(y) * (1.0 + x2y2);
	c2 = c1 - 2 * y2 + x2;
	p = x2y2 * x2y2;
	c3 = -2.0 * c1 + 1.0 + 2.0 * y2 + p;
	d = y2 / c3 + (2 * pow (c2 / c3, 3.0) - 9.0 * c1 * c2 / (c3 * c3)) / 27.0;
	a1 = (c1 - c2 * c2 / (3.0 * c3)) / c3;
	m1 = 2 * sqrt (-a1 / 3.0);
	theta1 = d_acos (3.0 * d / (a1 * m1)) / 3.0;

	*lat = copysign (180.0, y) * (-m1 * cos (theta1 + M_PI/3.0) - c2 / (3.0 * c3));
	*lon = project_info.central_meridian;
	if (x != 0.0) *lon += 90.0 * (x2y2 - 1.0 + sqrt (1.0 + 2 * (x2 - y2) + p)) / x; 
}

/* -JR WINKEL-TRIPEL MODIFIED AZIMUTHAL PROJECTION */

void GMT_vwinkel (double lon0, double scale)
{
	/* Set up Winkel Tripel projection */

	GMT_check_R_J (&lon0);
	project_info.r_cosphi1 = cosd (50.0+(28.0/60.0));
	project_info.central_meridian = lon0;
	project_info.w_r = 0.25 * (scale * project_info.M_PR_DEG * 360.0);	/* = Half the minor axis */
}

void GMT_winkel (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Winkel Tripel x/y */
	double C, D, x1, x2, y1, c, s;

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	lat *= D2R;
	lon *= (0.5 * D2R);

	/* Fist find Aitoff x/y */

	sincos (lat, &s, &c);
	D = d_acos (c * cos (lon));
	if (fabs (D) < GMT_CONV_LIMIT)
		x1 = y1 = 0.0;
	else {
		C = s / sin (D);
		x1 = copysign (D * d_sqrt (1.0 - C * C), lon);
		y1 = D * C;
	}

	/* Then get equirectangular projection */

	x2 = lon * project_info.r_cosphi1;
	/* y2 = lat; use directly in averaging below */

	/* Winkler is the average value */

	*x = project_info.EQ_RAD * (x1 + x2);
	*y = 0.5 * project_info.EQ_RAD * (y1 + lat);
}

void GMT_iwinkel (double *lon, double *lat, double x, double y)
{
	/* Convert Winkel Tripel x/y to lon/lat */
	/* Based on iterative solution published by:
	 * Ipbuker, 2002, Cartography & Geographical Information Science, 20, 1, 37-42.
	 */

	int n_iter = 0;
	double phi0, lambda0, sp, cp, s2p, sl, cl, sl2, cl2, C, D, sq_C, C_32;
	double f1, f2, df1dp, df1dl, df2dp, df2dl, denom, delta;

	x *= project_info.i_EQ_RAD;
	y *= project_info.i_EQ_RAD;
	*lat = y / M_PI;	/* Initial guesses for lon and lat */
	*lon = x / M_PI;
	do {
		phi0 = *lat;
		lambda0 = *lon;
		sincos (phi0, &sp, &cp);
		sincos (lambda0, &sl, &cl);
		sincos (0.5 * lambda0, &sl2, &cl2);
		s2p = sin (2.0 * phi0);
		D = acos (cp * cl2);
		C = 1.0 - cp * cp * cl2 * cl2;
		sq_C = sqrt (C);
		C_32 = C * sq_C;
		f1 = 0.5 * ((2.0 * D * cp * sl2) / sq_C + lambda0 * project_info.r_cosphi1) - x;
		f2 = 0.5 * (D * sp / sq_C + phi0) - y;
		df1dp = (0.25 * (sl * s2p) / C) - ((D * sp * sl2) / C_32);
		df1dl = 0.5 * ((cp * cp * sl2 * sl2) / C + (D * cp * cl2 * sp * sp) / C_32 + project_info.r_cosphi1);
		df2dp = 0.5 * ((sp * sp * cl2) / C + (D * (1.0 - cl2 * cl2) * cp) / C_32 + 1.0);
		df2dl = 0.125 * ((s2p * sl2) / C - (D * sp * cp * cp * sl) / C_32);
		denom = df1dp * df2dl - df2dp * df1dl;
		*lat = phi0 - (f1 * df2dl - f2 * df1dl) / denom;
		*lon = lambda0 - (f2 * df1dp - f1 * df2dp) / denom;
		delta = fabs (*lat - phi0) + fabs (*lon - lambda0);
		n_iter++;
	}
	while (delta > 1e-12 && n_iter < 100);
	*lat *= R2D;
	*lon = project_info.central_meridian + (*lon) * R2D;
	if (fabs (*lon) > 180.0) *lon = copysign (180.0, *lon);
}

void GMT_iwinkel_sub (double y, double *phi)
{	/* Valid only along meridian 180 degree from central meridian.  Used in left/right_winkel only */
	int n_iter = 0;
	double c, phi0, delta, sp, cp;

	c = 2.0 * y * project_info.i_EQ_RAD;
	*phi = y * project_info.i_EQ_RAD;
	do {
		phi0 = *phi;
		sincos (phi0, &sp, &cp);
		*phi = phi0 - (phi0 + M_PI_2 * sp - c) / (1.0 + M_PI_2 * cp);
		delta = fabs (*phi - phi0);
		n_iter++;
	}
	while (delta > GMT_CONV_LIMIT && n_iter < 100);
}

double GMT_left_winkel (double y)
{
	double c, phi, x;

	y -= project_info.y0;
	y *= project_info.i_y_scale;
	GMT_iwinkel_sub (y, &phi);
	GMT_geo_to_xy (project_info.central_meridian-180.0, phi * R2D, &x, &c);
	return (x);
}

double GMT_right_winkel (double y)
{
	double c, phi, x;

	y -= project_info.y0;
	y *= project_info.i_y_scale;
	GMT_iwinkel_sub (y, &phi);
	GMT_geo_to_xy (project_info.central_meridian+180.0, phi * R2D, &x, &c);
	return (x);
}

/* -JKf ECKERT IV PROJECTION */

void GMT_veckert4 (double lon0)
{
	/* Set up Eckert IV projection */

	GMT_check_R_J (&lon0);
	project_info.k4_x = 2.0 * project_info.EQ_RAD / d_sqrt (M_PI * (4.0 + M_PI));
	project_info.k4_y = 2.0 * project_info.EQ_RAD * d_sqrt (M_PI / (4.0 + M_PI));
	project_info.k4_ix = 1.0 / project_info.k4_x;
	project_info.k4_iy = 1.0 / project_info.k4_y;
	project_info.central_meridian = lon0;
}

void GMT_eckert4 (double lon, double lat, double *x, double *y)
{
	int n_iter = 0;
	double phi, delta, s_lat, s, c;
	/* Convert lon/lat to Eckert IV x/y */

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	lat *= D2R;
	lon *= D2R;
	phi = 0.5 * lat;
	s_lat = sin (lat);
	do {
		sincos (phi, &s, &c);
		delta = -(phi + s * c + 2.0 * s - (2.0 + M_PI_2) * s_lat) / (2.0 * c * (1.0 + c));
		phi += delta;
	}
	while (fabs(delta) > GMT_CONV_LIMIT && n_iter < 100);

	sincos (phi, &s, &c);
	*x = project_info.k4_x * lon * (1.0 + c);
	*y = project_info.k4_y * s;
}

void GMT_ieckert4 (double *lon, double *lat, double x, double y)
{
	/* Convert Eckert IV x/y to lon/lat */

	double phi, s, c;

	s = y * project_info.k4_iy;
	phi = d_asin (s);
	c = cos (phi);
	*lat = d_asin ((phi + s * c + 2.0 * s) / (2.0 + M_PI_2)) * R2D;
	*lon = project_info.central_meridian + R2D * x * project_info.k4_ix / (1.0 + c);
}

double GMT_left_eckert4 (double y)
{
	double x, phi;

	y -= project_info.y0;
	y *= project_info.i_y_scale;
	phi = d_asin (y * project_info.k4_iy);
	x = project_info.k4_x * D2R * (project_info.w - project_info.central_meridian) * (1.0 + cos (phi));
	return (x * project_info.x_scale + project_info.x0);
}

double GMT_right_eckert4 (double y)
{
	double x, phi;

	y -= project_info.y0;
	y *= project_info.i_y_scale;
	phi = d_asin (y * project_info.k4_iy);
	x = project_info.k4_x * D2R * (project_info.e - project_info.central_meridian) * (1.0 + cos (phi));
	return (x * project_info.x_scale + project_info.x0);
}

/* -JKs ECKERT VI PROJECTION */

void GMT_veckert6 (double lon0)
{
	/* Set up Eckert VI projection */

	GMT_check_R_J (&lon0);
	project_info.k6_r = project_info.EQ_RAD / sqrt (2.0 + M_PI);
	project_info.k6_ir = 1.0 / project_info.k6_r;
	project_info.central_meridian = lon0;
}

void GMT_eckert6 (double lon, double lat, double *x, double *y)
{
	int n_iter = 0;
	double phi, delta, s_lat, s, c;
	/* Convert lon/lat to Eckert VI x/y */

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_lata (lat);
	lat *= D2R;
	lon *= D2R;
	phi = lat;
	s_lat = sin (lat);
	do {
		sincos (phi, &s, &c);
		delta = -(phi + s - (1.0 + M_PI_2) * s_lat) / (1.0 + c);
		phi += delta;
	}
	while (fabs(delta) > GMT_CONV_LIMIT && n_iter < 100);

	*x = project_info.k6_r * lon * (1.0 + cos (phi));
	*y = 2.0 * project_info.k6_r * phi;
}

void GMT_ieckert6 (double *lon, double *lat, double x, double y)
{
	/* Convert Eckert VI x/y to lon/lat */

	double phi, c, s;

	phi = 0.5 * y * project_info.k6_ir;
	sincos (phi, &s, &c);
	*lat = d_asin ((phi + s) / (1.0 + M_PI_2)) * R2D;
	*lon = project_info.central_meridian + R2D * x * project_info.k6_ir / (1.0 + c);
	if (project_info.GMT_convert_latitudes) *lat = GMT_lata_to_latg (*lat);
}

double GMT_left_eckert6 (double y)
{
	double x, phi;

	y -= project_info.y0;
	y *= project_info.i_y_scale;
	phi = 0.5 * y * project_info.k6_ir;
	x = project_info.k6_r * D2R * (project_info.w - project_info.central_meridian) * (1.0 + cos (phi));
	return (x * project_info.x_scale + project_info.x0);
}

double GMT_right_eckert6 (double y)
{
	double x, phi;

	y -= project_info.y0;
	y *= project_info.i_y_scale;
	phi = 0.5 * y * project_info.k6_ir;
	x = project_info.k6_r * D2R * (project_info.e - project_info.central_meridian) * (1.0 + cos (phi));
	return (x * project_info.x_scale + project_info.x0);
}

/* -JN ROBINSON PSEUDOCYLINDRICAL PROJECTION */

void GMT_vrobinson (double lon0)
{
	int err_flag;

	/* Set up Robinson projection */

	if (gmtdefs.interpolant == 0) {	/* Must reset and warn */
		fprintf (stderr, "GMT Warning : -JN requires Akima or Cubic spline interpolant, set to Akima\n");
		gmtdefs.interpolant = 1;
	}

	GMT_check_R_J (&lon0);
	project_info.n_cx = 0.8487 * project_info.EQ_RAD * D2R;
	project_info.n_cy = 1.3523 * project_info.EQ_RAD;
	project_info.n_i_cy = 1.0 / project_info.n_cy;
	project_info.central_meridian = lon0;

	project_info.n_phi[0] = 0;	project_info.n_X[0] = 1.0000;	project_info.n_Y[0] = 0.0000;
	project_info.n_phi[1] = 5;	project_info.n_X[1] = 0.9986;	project_info.n_Y[1] = 0.0620;
	project_info.n_phi[2] = 10;	project_info.n_X[2] = 0.9954;	project_info.n_Y[2] = 0.1240;
	project_info.n_phi[3] = 15;	project_info.n_X[3] = 0.9900;	project_info.n_Y[3] = 0.1860;
	project_info.n_phi[4] = 20;	project_info.n_X[4] = 0.9822;	project_info.n_Y[4] = 0.2480;
	project_info.n_phi[5] = 25;	project_info.n_X[5] = 0.9730;	project_info.n_Y[5] = 0.3100;
	project_info.n_phi[6] = 30;	project_info.n_X[6] = 0.9600;	project_info.n_Y[6] = 0.3720;
	project_info.n_phi[7] = 35;	project_info.n_X[7] = 0.9427;	project_info.n_Y[7] = 0.4340;
	project_info.n_phi[8] = 40;	project_info.n_X[8] = 0.9216;	project_info.n_Y[8] = 0.4958;
	project_info.n_phi[9] = 45;	project_info.n_X[9] = 0.8962;	project_info.n_Y[9] = 0.5571;
	project_info.n_phi[10] = 50;	project_info.n_X[10] = 0.8679;	project_info.n_Y[10] = 0.6176;
	project_info.n_phi[11] = 55;	project_info.n_X[11] = 0.8350;	project_info.n_Y[11] = 0.6769;
	project_info.n_phi[12] = 60;	project_info.n_X[12] = 0.7986;	project_info.n_Y[12] = 0.7346;
	project_info.n_phi[13] = 65;	project_info.n_X[13] = 0.7597;	project_info.n_Y[13] = 0.7903;
	project_info.n_phi[14] = 70;	project_info.n_X[14] = 0.7186;	project_info.n_Y[14] = 0.8435;
	project_info.n_phi[15] = 75;	project_info.n_X[15] = 0.6732;	project_info.n_Y[15] = 0.8936;
	project_info.n_phi[16] = 80;	project_info.n_X[16] = 0.6213;	project_info.n_Y[16] = 0.9394;
	project_info.n_phi[17] = 85;	project_info.n_X[17] = 0.5722;	project_info.n_Y[17] = 0.9761;
	project_info.n_phi[18] = 90;	project_info.n_X[18] = 0.5322;	project_info.n_Y[18] = 1.0000;
	project_info.n_x_coeff  = (double *) GMT_memory (VNULL, (size_t)(3 * N_ROBINSON), sizeof (double), GMT_program);
	project_info.n_y_coeff  = (double *) GMT_memory (VNULL, (size_t)(3 * N_ROBINSON), sizeof (double), GMT_program);
	project_info.n_iy_coeff = (double *) GMT_memory (VNULL, (size_t)(3 * N_ROBINSON), sizeof (double), GMT_program);
	if (gmtdefs.interpolant == 2) {	/* Natural cubic spline */
		err_flag = GMT_cspline (project_info.n_phi, project_info.n_X, N_ROBINSON, project_info.n_x_coeff);
		err_flag = GMT_cspline (project_info.n_phi, project_info.n_Y, N_ROBINSON, project_info.n_y_coeff);
		err_flag = GMT_cspline (project_info.n_Y, project_info.n_phi, N_ROBINSON, project_info.n_iy_coeff);
	}
	else { 	/* Akimas spline */
		err_flag = GMT_akima (project_info.n_phi, project_info.n_X, N_ROBINSON, project_info.n_x_coeff);
		err_flag = GMT_akima (project_info.n_phi, project_info.n_Y, N_ROBINSON, project_info.n_y_coeff);
		err_flag = GMT_akima (project_info.n_Y, project_info.n_phi, N_ROBINSON, project_info.n_iy_coeff);
	}
	if (err_flag != 0) {
		fprintf (stderr, "GMT ERROR: -JN initialization of spline failed - report to GMT gurus\n");
		exit (EXIT_FAILURE);
	}

}

void GMT_robinson (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Robinson x/y */
	double tmp, X, Y;

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	tmp = fabs (lat);

	X = GMT_robinson_spline (tmp, project_info.n_phi, project_info.n_X, project_info.n_x_coeff);
	Y = GMT_robinson_spline (tmp, project_info.n_phi, project_info.n_Y, project_info.n_y_coeff);
	*x = project_info.n_cx * X * lon;	/* D2R is in n_cx already */
	*y = project_info.n_cy * copysign (Y, lat);
}

void GMT_irobinson (double *lon, double *lat, double x, double y)
{
	/* Convert Robinson x/y to lon/lat */
	double X, Y;

	Y = fabs (y * project_info.n_i_cy);
	*lat = GMT_robinson_spline (Y, project_info.n_Y, project_info.n_phi, project_info.n_iy_coeff);
	X = GMT_robinson_spline (*lat, project_info.n_phi, project_info.n_X, project_info.n_x_coeff);

	if (y < 0.0) *lat = -(*lat);
	*lon = x / (project_info.n_cx * X) + project_info.central_meridian;
}

double GMT_robinson_spline (double xp, double *x, double *y, double *c) {
	/* Compute the interpolated values from the Robinson coefficients */

	int j = 0, j1;
	double yp, a, b, h, ih, dx;

	if (xp < x[0] || xp > x[N_ROBINSON-1])	/* Desired point outside data range */
		return (GMT_d_NaN);

	while (j < N_ROBINSON && x[j] <= xp) j++;
	if (j == N_ROBINSON) j--;
	if (j > 0) j--;

	dx = xp - x[j];
	switch (gmtdefs.interpolant) {	/* GMT_vrobinson would not allow case 0 so only 1 | 2 is possible */
		case 1:
			yp = ((c[3*j+2]*dx + c[3*j+1])*dx + c[3*j])*dx + y[j];
			break;
		case 2:
			j1 = j + 1;
			h = x[j1] - x[j];
			ih = 1.0 / h;
			a = (x[j1] - xp) * ih;
			b = dx * ih;
			yp = a * y[j] + b * y[j1] + ((a*a*a - a) * c[j] + (b*b*b - b) * c[j1]) * (h*h) / 6.0;
			break;
		default:
			yp = 0;
	}
	return (yp);
}

double GMT_left_robinson (double y)
{
	double x, X, Y;

	y -= project_info.y0;
	y *= project_info.i_y_scale;
	Y = fabs (y * project_info.n_i_cy);
	GMT_intpol (project_info.n_Y, project_info.n_X, 19, 1, &Y, &X, gmtdefs.interpolant);

	x = project_info.n_cx * X * (project_info.w - project_info.central_meridian);
	return (x * project_info.x_scale + project_info.x0);
}

double GMT_right_robinson (double y)
{
	double x, X, Y;

	y -= project_info.y0;
	y *= project_info.i_y_scale;
	Y = fabs (y * project_info.n_i_cy);
	GMT_intpol (project_info.n_Y, project_info.n_X, 19, 1, &Y, &X, gmtdefs.interpolant);

	x = project_info.n_cx * X * (project_info.e - project_info.central_meridian);
	return (x * project_info.x_scale + project_info.x0);
}

/* -JI SINUSOIDAL EQUAL AREA PROJECTION */

void GMT_vsinusoidal (double lon0)
{
	/* Set up Sinusoidal projection */

	GMT_check_R_J (&lon0);
	project_info.central_meridian = lon0;
}

void GMT_sinusoidal (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Sinusoidal Equal-Area x/y */

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_lata (lat);

	lon *= D2R;
	lat *= D2R;

	*x = project_info.EQ_RAD * lon * cos (lat);
	*y = project_info.EQ_RAD * lat;
}

void GMT_isinusoidal (double *lon, double *lat, double x, double y)
{
	/* Convert Sinusoidal Equal-Area x/y to lon/lat */

	*lat = y * project_info.i_EQ_RAD;
	*lon = ((fabs (fabs (*lat) - M_PI) < GMT_CONV_LIMIT) ? 0.0 : R2D * x / (project_info.EQ_RAD * cos (*lat))) + project_info.central_meridian;
	*lat *= R2D;
	if (project_info.GMT_convert_latitudes) *lat = GMT_lata_to_latg (*lat);
}

double GMT_left_sinusoidal (double y)
{
	double x, lat;
	y -= project_info.y0;
	y *= project_info.i_y_scale;
	lat = y * project_info.i_EQ_RAD;
	x = project_info.EQ_RAD * (project_info.w - project_info.central_meridian) * D2R * cos (lat);
	return (x * project_info.x_scale + project_info.x0);
}

double GMT_right_sinusoidal (double y)
{
	double x, lat;
	y -= project_info.y0;
	y *= project_info.i_y_scale;
	lat = y * project_info.i_EQ_RAD;
	x = project_info.EQ_RAD * (project_info.e - project_info.central_meridian) * D2R * cos (lat);
	return (x * project_info.x_scale + project_info.x0);
}

/* -JC CASSINI PROJECTION */

void GMT_vcassini (double lon0, double lat0)
{
	/* Set up Cassini projection */

	double e1, lat2, s2, c2;

	GMT_check_R_J (&lon0);
	project_info.central_meridian = lon0;
	project_info.pole = lat0;
	lat0 *= D2R;
	project_info.c_p = lat0;
	lat2 = 2.0 * lat0;
	sincos (lat2, &s2, &c2);

	e1 = (1.0 - d_sqrt (project_info.one_m_ECC2)) / (1.0 + d_sqrt (project_info.one_m_ECC2));

	project_info.c_c1 = 1.0 - (1.0/4.0) * project_info.ECC2 - (3.0/64.0) * project_info.ECC4 - (5.0/256.0) * project_info.ECC6;
	project_info.c_c2 = -((3.0/8.0) * project_info.ECC2 + (3.0/32.0) * project_info.ECC4 + (25.0/768.0) * project_info.ECC6);
	project_info.c_c3 = (15.0/128.0) * project_info.ECC4 + (45.0/512.0) * project_info.ECC6;
	project_info.c_c4 = -(35.0/768.0) * project_info.ECC6;
	project_info.c_M0 = project_info.EQ_RAD * (project_info.c_c1 * lat0 + s2 * (project_info.c_c2 + c2 * (project_info.c_c3 + c2 * project_info.c_c4)));
	project_info.c_i1 = 1.0 / (project_info.EQ_RAD * project_info.c_c1);
	project_info.c_i2 = (3.0/2.0) * e1 - (29.0/12.0) * pow (e1, 3.0);
	project_info.c_i3 = (21.0/8.0) * e1 * e1 - (1537.0/128.0) * pow (e1, 4.0);
	project_info.c_i4 = (151.0/24.0) * pow (e1, 3.0);
	project_info.c_i5 = (1097.0/64.0) * pow (e1, 4.0);
}

void GMT_cassini (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Cassini x/y */

	double lat2, tany, N, T, A, C, M, s, c, s2, c2, A2, A3;

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	lon *= D2R;

	if (fabs (lat) < GMT_CONV_LIMIT) {	/* Quick when lat is zero */
		*x = project_info.EQ_RAD * lon;
		*y = -project_info.c_M0;
		return;
	}

	lat *= D2R;
	lat2 = 2.0 * lat;
	sincos (lat, &s, &c);
	sincos (lat2, &s2, &c2);
	tany = s / c;
	N = project_info.EQ_RAD / sqrt (1.0 - project_info.ECC2 * s * s);
	T = tany * tany;
	A = lon * c;
	A2 = A * A;
	A3 = A2 * A;
	C = project_info.ECC2 * c * c * project_info.i_one_m_ECC2;
	M = project_info.EQ_RAD * (project_info.c_c1 * lat + s2 * (project_info.c_c2 + c2 * (project_info.c_c3 + c2 * project_info.c_c4)));
	*x = N * (A - T * A3 / 6.0 - (8.0 - T + 8 * C) * T * A3 * A2 / 120.0);
	*y = M - project_info.c_M0 + N * tany * (0.5 * A2 + (5.0 - T + 6.0 * C) * A2 * A2 / 24.0);
}

void GMT_icassini (double *lon, double *lat, double x, double y)
{
	/* Convert Cassini x/y to lon/lat */

	double M1, u1, u2, s, c, phi1, tany, T1, N1, R_1, D, S2, D2, D3;

	M1 = project_info.c_M0 + y;
	u1 = M1 * project_info.c_i1;
	u2 = 2.0 * u1;
	sincos (u2, &s, &c);
	phi1 = u1 + s * (project_info.c_i2 + c * (project_info.c_i3 + c * (project_info.c_i4 + c * project_info.c_i5)));
	if (fabs (fabs (phi1) - M_PI_2) < GMT_CONV_LIMIT) {
		*lat = copysign (M_PI_2, phi1);
		*lon = project_info.central_meridian;
	}
	else {
		sincos (phi1, &s, &c);
		tany = s / c;
		T1 = tany * tany;
		S2 = 1.0 - project_info.ECC2 * s * s;
		N1 = project_info.EQ_RAD / sqrt (S2);
		R_1 = project_info.EQ_RAD * project_info.one_m_ECC2 / pow (S2, 1.5);
		D = x / N1;
		D2 = D * D;
		D3 = D2 * D;
		*lat = R2D * (phi1 - (N1 * tany / R_1) * (0.5 * D2 - (1.0 + 3.0 * T1) * D2 * D2 / 24.0));
		*lon = project_info.central_meridian + R2D * (D - T1 * D3 / 3.0 + (1.0 + 3.0 * T1) * T1 * D3 * D2 / 15.0) / c;
	}
}

/* Cassini functions for the Sphere */

void GMT_cassini_sph (double lon, double lat, double *x, double *y)
{
	double slon, clon, clat, tlat, slat;

	/* Convert lon/lat to Cassini x/y */

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	lon *= D2R;
	lat *= D2R;
	sincos (lon, &slon, &clon);
	sincos (lat, &slat, &clat);
	tlat = slat / clat;
	*x = project_info.EQ_RAD * d_asin (clat * slon);
	*y = project_info.EQ_RAD * (atan (tlat / clon) - project_info.c_p);
}

void GMT_icassini_sph (double *lon, double *lat, double x, double y)
{
	/* Convert Cassini x/y to lon/lat */

	double D, sD, cD, cx, tx, sx;

	x *= project_info.i_EQ_RAD;
	D = y * project_info.i_EQ_RAD + project_info.c_p;
	sincos (D, &sD, &cD);
	sincos (x, &sx, &cx);
	tx = sx / cx;
	*lat = R2D * d_asin (sD * cx);
	*lon = project_info.central_meridian + R2D * atan (tx / cD);
}

/* -JB ALBERS PROJECTION */

void GMT_valbers (double lon0, double lat0, double ph1, double ph2)
{
	/* Set up Albers projection */

	double s0, s1, s2, q0, q1, q2, m1, m2;

	GMT_check_R_J (&lon0);
	project_info.central_meridian = lon0;
	project_info.north_pole = (lat0 > 0.0);
	project_info.pole = (project_info.north_pole) ? 90.0 : -90.0;
	lat0 *= D2R;
	ph1 *= D2R;	ph2 *= D2R;

	s0 = sin (lat0);	s1 = sin (ph1);		s2 = sin (ph2);
	m1 = cos (ph1) * cos (ph1) / (1.0 - project_info.ECC2 * s1 * s1);	/* Actually m1 and m2 squared */
	m2 = cos (ph2) * cos (ph2) / (1.0 - project_info.ECC2 * s2 * s2);
	q0 = (fabs (project_info.ECC) < GMT_CONV_LIMIT) ? 2.0 * s0 : project_info.one_m_ECC2 * (s0 / (1.0 - project_info.ECC2 * s0 * s0) - project_info.i_half_ECC * log ((1.0 - project_info.ECC * s0) / (1.0 + project_info.ECC * s0)));
	q1 = (fabs (project_info.ECC) < GMT_CONV_LIMIT) ? 2.0 * s1 : project_info.one_m_ECC2 * (s1 / (1.0 - project_info.ECC2 * s1 * s1) - project_info.i_half_ECC * log ((1.0 - project_info.ECC * s1) / (1.0 + project_info.ECC * s1)));
	q2 = (fabs (project_info.ECC) < GMT_CONV_LIMIT) ? 2.0 * s2 : project_info.one_m_ECC2 * (s2 / (1.0 - project_info.ECC2 * s2 * s2) - project_info.i_half_ECC * log ((1.0 - project_info.ECC * s2) / (1.0 + project_info.ECC * s2)));

	project_info.a_n = (fabs (ph1 - ph2) < GMT_CONV_LIMIT) ? s1 : (m1 - m2) / (q2 - q1);
	project_info.a_i_n = 1.0 / project_info.a_n;
	project_info.a_C = m1 + project_info.a_n * q1;
	project_info.a_rho0 = project_info.EQ_RAD * sqrt (project_info.a_C - project_info.a_n * q0) * project_info.a_i_n;
	project_info.a_n2ir2 = (project_info.a_n * project_info.a_n) / (project_info.EQ_RAD * project_info.EQ_RAD);
	project_info.a_test = 1.0 - (project_info.i_half_ECC * project_info.one_m_ECC2) * log ((1.0 - project_info.ECC) / (1.0 + project_info.ECC));

}

void GMT_valbers_sph (double lon0, double lat0, double ph1, double ph2)
{
	/* Set up Spherical Albers projection (Snyder, page 100) */

	double s1, c1;

	GMT_check_R_J (&lon0);
	project_info.central_meridian = lon0;
	project_info.north_pole = (lat0 > 0.0);
	project_info.pole = (project_info.north_pole) ? 90.0 : -90.0;

	ph1 *= D2R;

	s1 = sin (ph1);		c1 = cos (ph1);

	project_info.a_n = (fabs (ph1 - ph2) < GMT_CONV_LIMIT) ? s1 : 0.5 * (s1 + sind (ph2));
	project_info.a_i_n = 1.0 / project_info.a_n;
	project_info.a_C = c1 * c1 + 2.0 * project_info.a_n * s1;
	project_info.a_rho0 = project_info.EQ_RAD * sqrt (project_info.a_C - 2.0 * project_info.a_n * sind (lat0)) * project_info.a_i_n;
	project_info.a_n2ir2 = 0.5 * project_info.a_n / (project_info.EQ_RAD * project_info.EQ_RAD);
	project_info.a_Cin = 0.5 * project_info.a_C / project_info.a_n;

}

void GMT_albers (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Albers x/y */

	double s, c, q, theta, rho, r;

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	lon *= D2R;
	lat *= D2R;

	s = sin (lat);
	if (fabs (project_info.ECC) < GMT_CONV_LIMIT)
		q = 2.0 * s;
	else {
		r = project_info.ECC * s;
		q = project_info.one_m_ECC2 * (s / (1.0 - project_info.ECC2 * s * s) - project_info.i_half_ECC * log ((1.0 - r) / (1.0 + r)));
	}
	theta = project_info.a_n * lon;
	rho = project_info.EQ_RAD * sqrt (project_info.a_C - project_info.a_n * q) * project_info.a_i_n;

	sincos (theta, &s, &c);
	*x = rho * s;
	*y = project_info.a_rho0 - rho * c;
}

void GMT_ialbers (double *lon, double *lat, double x, double y)
{
	/* Convert Albers x/y to lon/lat */

	int n_iter;
	double theta, rho, q, phi, phi0, s, c, s2, ex_1, delta, r;

	theta = (project_info.a_n < 0.0) ? d_atan2 (-x, y - project_info.a_rho0) : d_atan2 (x, project_info.a_rho0 - y);
	rho = hypot (x, project_info.a_rho0 - y);
	q = (project_info.a_C - rho * rho * project_info.a_n2ir2) * project_info.a_i_n;

	if (fabs (fabs (q) - project_info.a_test) < GMT_CONV_LIMIT)
		*lat = copysign (90.0, q);
	else {
		phi = d_asin (0.5 * q);
		n_iter = 0;
		do {
			phi0 = phi;
			sincos (phi0, &s, &c);
			r = project_info.ECC * s;
			s2 = s * s;
			ex_1 = 1.0 - project_info.ECC2 * s2;
			phi = phi0 + 0.5 * ex_1 * ex_1 * ((q * project_info.i_one_m_ECC2) - s / ex_1
				+ project_info.i_half_ECC * log ((1 - r) / (1.0 + r))) / c;
			delta = fabs (phi - phi0);
			n_iter++;
		}
		while (delta > GMT_CONV_LIMIT && n_iter < 100);
		*lat = R2D * phi;
	}
	*lon = project_info.central_meridian + R2D * theta * project_info.a_i_n;
}

/* Spherical versions of Albers */

void GMT_albers_sph (double lon, double lat, double *x, double *y)
{
	/* Convert lon/lat to Spherical Albers x/y */

	double s, c, theta, rho;

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	if (project_info.GMT_convert_latitudes) lat = GMT_latg_to_lata (lat);
	lon *= D2R;
	lat *= D2R;

	theta = project_info.a_n * lon;
	rho = project_info.EQ_RAD * sqrt (project_info.a_C - 2.0 * project_info.a_n * sin (lat)) * project_info.a_i_n;

	sincos (theta, &s, &c);
	*x = rho * s;
	*y = project_info.a_rho0 - rho * c;
}

void GMT_ialbers_sph (double *lon, double *lat, double x, double y)
{
	/* Convert Spherical Albers x/y to lon/lat */

	double theta, A, dy;

	theta = (project_info.a_n < 0.0) ? d_atan2 (-x, y - project_info.a_rho0) : d_atan2 (x, project_info.a_rho0 - y);
	dy = project_info.a_rho0 - y;
	A = (x * x + dy * dy) * project_info.a_n2ir2;

	*lat = R2D * d_asin (project_info.a_Cin - A);
	*lon = project_info.central_meridian + R2D * theta * project_info.a_i_n;
	if (project_info.GMT_convert_latitudes) *lat = GMT_lata_to_latg (*lat);
}

/* -JD EQUIDISTANT CONIC PROJECTION */

void GMT_veconic (double lon0, double lat0, double lat1, double lat2)
{
	double c1;

	/* Set up Equidistant Conic projection */

	GMT_check_R_J (&lon0);
	project_info.north_pole = (lat0 > 0.0);
	c1 = cosd (lat1);
	project_info.d_n = (fabs (lat1 - lat2) < GMT_CONV_LIMIT) ? sind (lat1) : (c1 - cosd (lat2)) / (D2R * (lat2 - lat1));
	project_info.d_i_n = R2D / project_info.d_n;	/* R2D put here instead of in lon for ieconic */
	project_info.d_G = (c1 / project_info.d_n) + lat1 * D2R;
	project_info.d_rho0 = project_info.EQ_RAD * (project_info.d_G - lat0 * D2R);
	project_info.central_meridian = lon0;
}

void GMT_econic (double lon, double lat, double *x, double *y)
{
	double rho, theta, s, c;
	/* Convert lon/lat to Equidistant Conic x/y */

	lon -= project_info.central_meridian;
	while (lon < -180.0) lon += 360.0;
	while (lon > 180.0) lon -= 360.0;
	lat *= D2R;
	lon *= D2R;
	rho = project_info.EQ_RAD * (project_info.d_G - lat);
	theta = project_info.d_n * lon;

	sincos (theta, &s, &c);
	*x = rho * s;
	*y = project_info.d_rho0 - rho * c;
}

void GMT_ieconic (double *lon, double *lat, double x, double y)
{
	/* Convert Equidistant Conic x/y to lon/lat */

	double rho, theta;

	rho = hypot (x, project_info.d_rho0 - y);
	if (project_info.d_n < 0.0) rho = -rho;
	theta = (project_info.d_n < 0.0) ? d_atan2 (-x, y - project_info.d_rho0) : d_atan2 (x, project_info.d_rho0 - y);
	*lat = (project_info.d_G - rho * project_info.i_EQ_RAD) * R2D;
	*lon = project_info.central_meridian + theta * project_info.d_i_n;
}
