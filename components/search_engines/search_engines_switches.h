// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_SWITCHES_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_SWITCHES_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace switches {

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const char kExtraSearchQueryParams[];

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const char kSearchEngineChoiceCountry[];

// `kDefaultListCountryOverride` and `kEeaRegionCountryOverrideString` are
// special values for `kSearchEngineChoiceCountry`.
// `kDefaultListCountryOverride` will override the list of search engines to
// display the default set.
// `kEeaListCountryOverride` will override the list
// of search engines to display list of all EEA engines.
inline const char kDefaultListCountryOverride[] = "DEFAULT_EEA";
inline const char kEeaListCountryOverride[] = "EEA_ALL";

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const char kIgnoreNoFirstRunForSearchEngineChoiceScreen[];

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const char kDisableSearchEngineChoiceScreen[];

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const char kForceSearchEngineChoiceScreen[];

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_DECLARE_FEATURE(kSearchEngineChoiceGuestExperience);

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_DECLARE_FEATURE(kSearchEngineChoiceTrigger);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_DECLARE_FEATURE(kRemoveSearchEngineChoiceAttribution);
#endif

// The string that's passed to
// `switches::kSearchEngineChoiceTriggerRepromptParams` so that we don't
// reprompt users with the choice screen.
inline constexpr char kSearchEngineChoiceNoRepromptString[] = "NO_REPROMPT";

// Reprompt params for the search engine choice.
// This is a JSON dictionary where keys are country codes, and values are Chrome
// version strings. The wildcard country '*' represents all countries.
// When a specific country is specified, it takes precedence over the wildcard.
//
// Example: {"*": "2.0.0.0", "BE": "5.0.0.0"}
// This reprompts users in Belgium who made the choice strictly before version
// 5, and users in all other countries who made the choice strictly before
// version 2.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const base::FeatureParam<std::string>
    kSearchEngineChoiceTriggerRepromptParams;

#if BUILDFLAG(IS_IOS)
// Maximum number of time the search engine choice screen can be skipped
// because the application is started via an external intent. Once this
// count is reached, the search engine choice screen is presented on all
// restart until the user has made a decision.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const base::FeatureParam<int> kSearchEngineChoiceMaximumSkipCount;
#endif

#if BUILDFLAG(IS_ANDROID)
// Enables the blocking dialog that directs users to complete their choice of
// default apps (for Browser & Search) in Android.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_DECLARE_FEATURE(kClayBlocking);
#endif

// Kill switch to revert the fix of using assistedQueryStats for prefetch source
// component. See crbug.com/345275145.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_DECLARE_FEATURE(kPrefetchParameterFix);

// Kill switch to revert the fix of dropping searchbox stats (gs_lcrp) from
// prefetch requests. See crbug.com/350939001.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_DECLARE_FEATURE(kRemoveSearchboxStatsParamFromPrefetchRequests);

// Switch guarding TemplateURL reconciliation mechanism.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_DECLARE_FEATURE(kTemplateUrlReconciliation);

// Parameter associated with kTemplateUrlReconciliation flag.
// When set to <true>, reconciliation is performed with all known Search Engine
// definitions.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const base::FeatureParam<bool> kReconcileWithAllKnownEngines;

}  // namespace switches

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_SWITCHES_H_
