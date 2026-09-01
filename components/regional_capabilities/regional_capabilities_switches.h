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

#if BUILDFLAG(IS_IOS)
// Enables the Taiyaki regional program on all surfaces, including post-FRE
// surfaces. When disabled, Taiyaki is only enabled on the FRE.
BASE_DECLARE_FEATURE(kTaiyakiAllSurfaces);

// Feature flag for SearchEngineChoiceScreenSnackbar.
BASE_DECLARE_FEATURE(kSearchEngineChoiceScreenSnackbar);

// Returns true if SearchEngineChoiceScreenSnackbar is enabled.
bool IsSearchEngineChoiceScreenSnackbarEnabled();
#endif  // BUILDFLAG(IS_IOS)

// Returns true if the dynamic profile country feature is enabled.
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
bool IsDynamicProfileCountryEnabled();
#else
// Always returns true on iOS and Android.
consteval bool IsDynamicProfileCountryEnabled() {
  return true;
}
#endif

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
// Updates profile country preference stored in preferences
// dynamically when the current country does not match the stored value.
BASE_DECLARE_FEATURE(kDynamicProfileCountry);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

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

// Guards the incremental rollout of the feature that enables migrating
// prepopulated engines.
// When enabled, also enable `kApplySearchEngineTypeMigration`.
// Note: Due to the migration changing the client's data locally persisted in
// various places, we don't support rollbacks to the feature state.
BASE_DECLARE_FEATURE(kPrepopulatedEnginesMigration);

// Whether some search engine variants are explicitly assigned to a specific
// region even if they are not part of the top regional engines list. When
// enabled, also enable `kApplySearchEngineTypeMigration`.
BASE_DECLARE_FEATURE(kPrepopulatedEnginesShadowVariants);

bool ArePrepopulatedEnginesShadowVariantsEnabled();

// When enabled, resolves prepopulated engines undergoing an ID split (e.g.
// Yahoo! JAPAN) to their post-migration SearchEngineType.
// Companion feature to `kPrepopulatedEnginesMigration` and
// `kPrepopulatedEnginesShadowVariants`, should be enabled when any of them is
// also enabled.
BASE_DECLARE_FEATURE(kApplySearchEngineTypeMigration);

}  // namespace switches

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_
