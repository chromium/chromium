// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engines_switches.h"

#include "base/feature_list.h"

namespace switches {

// Additional query params to insert in the search and instant URLs.  Useful for
// testing.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
const char kExtraSearchQueryParams[] = "extra-search-query-params";

// Override the country used for search engine choice region checks.
// Intended for testing. Expects 2-letter country codes.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
const char kSearchEngineChoiceCountry[] = "search-engine-choice-country";

// Disable the search engine choice screen for testing / autmation.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
const char kDisableSearchEngineChoiceScreen[] =
    "disable-search-engine-choice-screen";

// Force-enable showing the search engine choice screen for testing regardless
// of region or choice already having been made.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
const char kForceSearchEngineChoiceScreen[] =
    "force-search-engine-choice-screen";

// Enables the search engine choice screen. Feature parameters below can
// affect the actual triggering logic.
// The default feature state is split by platform to ease potential merges
// that could be needed if we need to change the state while waterfalling this
// feature.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_FEATURE(kSearchEngineChoiceTrigger,
             "SearchEngineChoiceTrigger",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_FEATURE(kSearchEngineChoiceAttribution,
             "SearchEngineChoiceAttribution",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_FEATURE(kSearchEnginesSortingCleanup,
             "kSearchEnginesSortingCleanup",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPersistentSearchEngineChoiceImport,
             "PersistentSearchEngineChoiceImport",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
const base::FeatureParam<bool> kSearchEngineChoiceTriggerWithForceEeaCountry{
    &kSearchEngineChoiceTrigger, /*name=*/"with_force_eea_country", false};

// Use an explicit "NO_REPROMPT" value as default to avoid reprompting users
// who saw the choice screen in M121.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
const base::FeatureParam<std::string> kSearchEngineChoiceTriggerRepromptParams{
    &kSearchEngineChoiceTrigger,
    /*name=*/"reprompt",
    /*default_value=*/kSearchEngineChoiceNoRepromptString};

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
const base::FeatureParam<bool> kSearchEngineChoiceTriggerSkipFor3p{
    &kSearchEngineChoiceTrigger,
    /*name=*/"skip_for_3p",
    /*default_value=*/true};

#if BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const base::FeatureParam<int> kSearchEngineChoiceMaximumSkipCount{
    &kSearchEngineChoiceTrigger,
    /*name=*/"maximum_skip_count",
    /*default_value=*/10};
#endif

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSearchEngineChoice,
             "SearchEngineChoice",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSearchEnginePromoDialogRewrite,
             "SearchEnginePromoDialogRewrite",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_FEATURE(kPrefetchParameterFix,
             "PrefetchParameterFix",
             base::FEATURE_ENABLED_BY_DEFAULT);

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_FEATURE(kRemoveSearchboxStatsParamFromPrefetchRequests,
             "RemoveSearchboxStatsParamFromPrefetchRequests",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace switches
