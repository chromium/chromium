// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"

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

#if BUILDFLAG(IS_ANDROID)
// Mitigate overlap cases between the legacy search engine promo and the
// device-based program eligibility determinations.
BASE_DECLARE_FEATURE(kMitigateLegacySearchEnginePromoOverlap);
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// Use finch permanent country instead of finch latest country for fetching
// country ID.
BASE_DECLARE_FEATURE(kUseFinchPermanentCountryForFetchCountryId);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(REGIONAL_CAPABILITIES_SWITCHES)
BASE_DECLARE_FEATURE(kTaiyaki);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// Updates profile country preference stored in preferences
// dynamically when the current country does not match the stored value.
BASE_DECLARE_FEATURE(kDynamicProfileCountry);

}  // namespace switches

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_
