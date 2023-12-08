// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engines_switches.h"

namespace switches {

// Additional query params to insert in the search and instant URLs.  Useful for
// testing.
const char kExtraSearchQueryParams[] = "extra-search-query-params";

// Override the country used for search engine choice region checks.
// Intended for testing. Expects 2-letter country codes.
const char kSearchEngineChoiceCountry[] = "search-engine-choice-country";

// Disable the search engine choice screen for testing / autmation.
const char kDisableSearchEngineChoiceScreen[] =
    "disable-search-engine-choice-screen";

// Force-enable showing the search engine choice screen for testing regardless
// of region or choice already having been made.
const char kForceSearchEngineChoiceScreen[] =
    "force-search-engine-choice-screen";

BASE_FEATURE(kSearchEngineChoiceTrigger,
             "SearchEngineChoiceTrigger",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kSearchEngineChoiceTriggerForTaggedProfilesOnly{
    &kSearchEngineChoiceTrigger, /*name=*/"for_tagged_profiles_only", false};

const base::FeatureParam<bool> kSearchEngineChoiceTriggerWithForceEeaCountry{
    &kSearchEngineChoiceTrigger, /*name=*/"with_force_eea_country", false};

const base::FeatureParam<std::string> kSearchEngineChoiceTriggerRepromptParams{
    &kSearchEngineChoiceTrigger,
    /*name=*/"reprompt",
    /*default_value=*/"{}"};

}  // namespace switches
