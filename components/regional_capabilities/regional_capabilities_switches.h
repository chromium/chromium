// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace switches {

// Overrides the profile country (which is among other things used for search
// engine choice region checks for example).
// Intended for testing. Expects 2-letter country codes.
inline constexpr char kSearchEngineChoiceCountry[] =
    "search-engine-choice-country";

// `kDefaultListCountryOverride` and `kEeaRegionCountryOverrideString` are
// special values for `kSearchEngineChoiceCountry`.
// `kDefaultListCountryOverride` will override the list of search engines to
// display the default set.
// `kEeaListCountryOverride` will override the list
// of search engines to display list of all EEA engines.
inline constexpr char kDefaultListCountryOverride[] = "DEFAULT_EEA";
inline constexpr char kEeaListCountryOverride[] = "EEA_ALL";

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// When an invalid `country_codes::CountryId` is stored in prefs and this
// feature is enabled the pref will be cleared allowing a valid country to be
// set again.
BASE_DECLARE_FEATURE(kClearPrefForUnknownCountry);
#endif
}  // namespace switches

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_
