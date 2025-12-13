// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace switches {

// Overrides the profile country (which is among other things used for search
// engine choice region checks for example).
// Intended for testing. Parameter can be one of 3 things:
// - 2-letter country codes => Will override the profile country
// - A program name => Will override the country and the program
// - A specific list override => Will override the program, but instead of
// overriding the country, will use special values to force the search engine
// list to some preset testing ones.
inline constexpr char kSearchEngineChoiceCountry[] =
    "search-engine-choice-country";

// Special value for the `kSearchEngineChoiceCountry` command-line flag. Enables
// the Taiyaki program. On unsupported platform / build types, will fall back to
// default program / unknown country.
inline constexpr char kTaiyakiProgramOverride[] = "TAIYAKI";

// Special value for the `kSearchEngineChoiceCountry` command-line flag. Enables
// the Waffle program and overrides the list of search engines to display the
// default set.
inline constexpr char kDefaultListCountryOverride[] = "DEFAULT_EEA";

// Special value for the `kSearchEngineChoiceCountry` command-line flag. Enables
// the Waffle program and overrides the list of search engines to display the
// list of all EEA engines.
inline constexpr char kEeaListCountryOverride[] = "EEA_ALL";

#if BUILDFLAG(IS_ANDROID)
// Ensure that the legacy search engine promos don't trigger on out of
// scope device types.
BASE_DECLARE_FEATURE(kRestrictLegacySearchEnginePromoOnFormFactors);

// Obtains the active regional program directly from the device instead of
// deriving it from the profile country. Kill switch, enabled by default.
BASE_DECLARE_FEATURE(kResolveRegionalCapabilitiesFromDevice);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// Describes UI surfaces that can receive the choice screen.
enum class RegionalCapabilitiesChoiceScreenSurface : int {
  // The choice screen should always be shown.
  kAll = 0,
  // The choice screen should only be shown in FRE.
  kInFreOnly = 1,
};

COMPONENT_EXPORT(REGIONAL_CAPABILITIES_SWITCHES)
BASE_DECLARE_FEATURE(kTaiyaki);

// For kTaiyaki enabled, defines which UI surfaces the choice screen can be
// shown on. Only used if kTaiyaki is enabled.
extern const base::FeatureParam<RegionalCapabilitiesChoiceScreenSurface>
    kTaiyakiChoiceScreenSurface;

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// Updates profile country preference stored in preferences
// dynamically when the current country does not match the stored value.
BASE_DECLARE_FEATURE(kDynamicProfileCountry);

// Whether support for showing the current default in the choice screen should
// be enabled. When enabled, the associated program settings will be read to
// determine whether to actually show it.
BASE_DECLARE_FEATURE(kCurrentDseHighlightOnChoiceScreenSupport);

// Whether to enable eligibility based on the current location for Waffle choice
// screens (see ChoiceScreenEligibilityConfig.restrict_to_associated_countries).
BASE_DECLARE_FEATURE(kWaffleRestrictToAssociatedCountries);

// For programs with restrict_to_associated_countries, whether an exact country
// match is required (in addition to a region match).
BASE_DECLARE_FEATURE(kStrictAssociatedCountriesCheck);

}  // namespace switches

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_
