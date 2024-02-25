// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_SWITCHES_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_SWITCHES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace switches {

extern const char kExtraSearchQueryParams[];

extern const char kSearchEngineChoiceCountry[];

extern const char kDisableSearchEngineChoiceScreen[];

extern const char kForceSearchEngineChoiceScreen[];

BASE_DECLARE_FEATURE(kSearchEngineChoiceTrigger);
extern const base::FeatureParam<bool>
    kSearchEngineChoiceTriggerForTaggedProfilesOnly;

// Forces the search engine choice country to Belgium. Used for testing
// purposes.
extern const base::FeatureParam<bool>
    kSearchEngineChoiceTriggerWithForceEeaCountry;

// Reprompt params for the search engine choice.
// This is a JSON dictionary where keys are country codes, and values are Chrome
// version strings. The wildcard country '*' represents all countries.
// When a specific country is specified, it takes precedence over the wildcard.
// Note: this has no effect for users with the parameter
// `kSearchEngineChoiceTriggerForTaggedProfilesOnly` set to `true`.
//
// Example: {"*": "2.0.0.0", "BE": "5.0.0.0"}
// This reprompts users in Belgium who made the choice strictly before version
// 5, and users in all other countries who made the choice strictly before
// version 2.
extern const base::FeatureParam<std::string>
    kSearchEngineChoiceTriggerRepromptParams;
}  // namespace switches

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_SWITCHES_H_
