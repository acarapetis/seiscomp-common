/***************************************************************************
 * Copyright (C) gempa GmbH                                                *
 * All rights reserved.                                                    *
 * Contact: gempa GmbH (seiscomp-dev@gempa.de)                             *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 *                                                                         *
 * Other Usage                                                             *
 * Alternatively, this file may be used in accordance with the terms and   *
 * conditions contained in a signed written agreement between you and      *
 * gempa GmbH.                                                             *
 ***************************************************************************/


#define SEISCOMP_COMPONENT MLc

#include <seiscomp/math/geo.h>
#include <seiscomp/processing/magnitudes/MLc.h>
#include <seiscomp/config/config.h>
#include <seiscomp/logging/log.h>


using namespace std;


namespace Seiscomp {
namespace Processing {


namespace {

std::string ExpectedAmplitudeUnit = "mm";

DEFINE_SMARTPOINTER(ExtraLocale);
class ExtraLocale : public Core::BaseObject {
	public:
		// parametric coefficients
		OPT(double) c0;
		OPT(double) c1;
		OPT(double) c2;
		OPT(double) c3;
		OPT(double) c4;
		OPT(double) c5;
		// A0, non-parametric coefficients
		OPT(LogA0)  logA0;
};

}


REGISTER_MAGNITUDEPROCESSOR(MagnitudeProcessor_MLc, "MLc");
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
MagnitudeProcessor_MLc::MagnitudeProcessor_MLc()
: Processing::MagnitudeProcessor("MLc") {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
std::string MagnitudeProcessor_MLc::amplitudeType() const {
	return "MLc";
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MagnitudeProcessor_MLc::setup(const Settings &settings) {
	if ( !MagnitudeProcessor::setup(settings) )
		return false;

	// depth constraint
	try {_maxDepth = settings.getDouble("magnitudes." + _type + ".maxDepth"); }
	catch ( ... ) {}

	// distance constraints
	try {_distanceMode = settings.getString("magnitudes." + _type + ".distMode"); }
	catch ( ... ) {}
	try {_minDistanceKm = Math::Geo::deg2km(settings.getDouble("magnitudes." + _type + ".minDist")); }
	catch ( ... ) {}
	try {_maxDistanceKm = Math::Geo::deg2km(settings.getDouble("magnitudes." + _type + ".maxDist")); }
	catch ( ... ) {}

	// calibration function
	try {_calibrationType = settings.getString("magnitudes." + _type + ".calibrationType"); }
	catch ( ... ) {}

	if ( (_calibrationType != "A0") && (_calibrationType != "parametric") ) {
		SEISCOMP_ERROR("%s: unrecognized calibration type: %s", _type.c_str(),
		               _calibrationType.c_str());
		return false;
	}

	// parametric calibration function
	try { _c0 = settings.getDouble("magnitudes." + _type + ".parametric.c0"); }
	catch ( ... ) {}
	try { _c1 = settings.getDouble("magnitudes." + _type + ".parametric.c1"); }
	catch ( ... ) {}
	try { _c2 = settings.getDouble("magnitudes." + _type + ".parametric.c2"); }
	catch ( ... ) {}
	try { _c3 = settings.getDouble("magnitudes." + _type + ".parametric.c3"); }
	catch ( ... ) {}
	try { _c4 = settings.getDouble("magnitudes." + _type + ".parametric.c4"); }
	catch ( ... ) {}
	try { _c5 = settings.getDouble("magnitudes." + _type + ".parametric.c5"); }
	catch ( ... ) {}

	// A0, non-parametric calibration function
	std::string defLogA0 = "0:-1.3,60:-2.8,100:-3.0,400:-4.5,1000:-5.85";
	try { defLogA0 = settings.getString("magnitudes." + _type + ".A0.logA0"); }
	catch ( ... ) {}

	// set distance-dependent intervals
	if ( !_logA0.set(defLogA0) ) {
		SEISCOMP_ERROR("%s: incorrect correction term log(A0): %s", _type.c_str(),
		               defLogA0.c_str());
		return false;
	}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool MagnitudeProcessor_MLc::initLocale(Locale *locale,
                                       const Settings &settings,
                                       const string &configPrefix) {
	const Seiscomp::Config::Config *cfg = settings.localConfiguration;
	ExtraLocalePtr extra = new ExtraLocale;

	// parametric
	try {
		extra->c0 = cfg->getDouble(configPrefix + ".parametric.c0");
	}
	catch ( ... ) {}
	try {
		extra->c1 = cfg->getDouble(configPrefix + ".parametric.c1");
	}
	catch ( ... ) {}
	try {
		extra->c2 = cfg->getDouble(configPrefix + ".parametric.c2");
	}
	catch ( ... ) {}
	try {
		extra->c3 = cfg->getDouble(configPrefix + ".parametric.c3");
	}
	catch ( ... ) {}
	try {
		extra->c4 = cfg->getDouble(configPrefix + ".parametric.c4");
	}
	catch ( ... ) {}
	try {
		extra->c5 = cfg->getDouble(configPrefix + ".parametric.c5");
	}
	catch ( ... ) {}

	locale->extra = extra;

	// A0, non-parametric
	try {
		string logA0 = cfg->getString(configPrefix + "A0.logA0");
		if ( !logA0.empty() ) {
			extra->logA0 = LogA0();
			if ( !extra->logA0->set(logA0) ) {
				return false;
			}

			locale->extra = extra;
		}
	}
	catch ( ... ) {}

	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
MagnitudeProcessor::Status MagnitudeProcessor_MLc::computeMagnitude(
	double amplitude, const std::string &unit,
	double, double, double delta, double depth,
	const DataModel::Origin *, const DataModel::SensorLocation *,
	const DataModel::Amplitude *,
	const Locale *locale,
	double &value) {
	if ( amplitude <= 0 )
		return AmplitudeOutOfRange;

	// Check depth, clip negative depth to 0
	if ( depth > _maxDepth ) {
		return DepthOutOfRange;
	}
	if ( depth < 0 ) depth = 0;

	double distanceKm;
	if ( _distanceMode == "hypocentral" ) {
		distanceKm = sqrt(pow(Math::Geo::deg2km(delta),2) + pow(depth,2));
	}
	else {
		distanceKm = Math::Geo::deg2km(delta);
	}

	if ( _maxDistanceKm >= 0 && distanceKm > _maxDistanceKm ) {
		return DistanceOutOfRange;
	}

	if ( _minDistanceKm >= 0 && distanceKm < _minDistanceKm ) {
		return DistanceOutOfRange;
	}

	if ( !convertAmplitude(amplitude, unit, ExpectedAmplitudeUnit) ) {
		return InvalidAmplitudeUnit;
	}

	ExtraLocale *extra = nullptr;
	if ( locale ) {
		extra = static_cast<ExtraLocale*>(locale->extra.get());
	}

	if ( _calibrationType == "parametric" ) {
		// parametric calibration function
		auto c0 = (extra and extra->c0) ? *extra->c0 : _c0;
		auto c1 = (extra and extra->c1) ? *extra->c1 : _c1;
		auto c2 = (extra and extra->c2) ? *extra->c2 : _c2;
		auto c3 = (extra and extra->c3) ? *extra->c3 : _c3;
		auto c4 = (extra and extra->c4) ? *extra->c4 : _c4;
		auto c5 = (extra and extra->c5) ? *extra->c5 : _c5;

		value = log10(amplitude)
		        + c3 * log10(distanceKm / c5)
		        + c2 * (distanceKm + c4)
		        + c1
		        + c0;
	}
	else if ( _calibrationType == "A0" ) {
		// A0, non-parametric calibration function

		try {
			value = log10(amplitude) - (extra and extra->logA0 ? extra->logA0->at(distanceKm) : _logA0.at(distanceKm));
		}
		catch ( Core::ValueException & ) {
			return DistanceOutOfRange;
		}
	}

	else {
		return IncompleteConfiguration;
	}

	return OK;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
}
}
