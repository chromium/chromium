// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_SWITCHES_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_SWITCHES_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace switches {

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const char kExtraSearchQueryParams[];

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const char kSearchEngineChoiceCountry[];

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const char kDisableSearchEngineChoiceScreen[];

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const char kForceSearchEngineChoiceScreen[];

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
BASE_DECLARE_FEATURE(kSearchEngineChoiceTrigger);

COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const base::FeatureParam<bool>
    kSearchEngineChoiceTriggerForTaggedProfilesOnly;

// Forces the search engine choice country to Belgium. Used for testing
// purposes.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
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
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const base::FeatureParam<std::string>
    kSearchEngineChoiceTriggerRepromptParams;

// Whether the search engine choice screen should be suppressed when the
// default search engine is not Google.
COMPONENT_EXPORT(SEARCH_ENGINES_SWITCHES)
extern const base::FeatureParam<bool> kSearchEngineChoiceTriggerSkipFor3p;
}  // namespace switches

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_SWITCHES_H_
