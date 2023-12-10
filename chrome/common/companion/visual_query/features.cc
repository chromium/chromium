// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/companion/visual_query/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace companion::visual_query::features {

BASE_FEATURE(kVisualQuerySuggestions,
             "VisualQuerySuggestions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kVisualQuerySuggestionsAgent,
             "VisualQuerySuggestionsAgent",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsVisualQuerySuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kVisualQuerySuggestions);
}

bool IsVisualQuerySuggestionsAgentEnabled() {
  return base::FeatureList::IsEnabled(kVisualQuerySuggestionsAgent);
}

base::TimeDelta StartClassificationRetryDuration() {
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      kVisualQuerySuggestions, "classification_retry_delay_ms", 2000));
}

int MaxVisualSuggestions() {
  return GetFieldTrialParamByFeatureAsInt(kVisualQuerySuggestions,
                                          "max_visual_suggestions", 1);
}

}  // namespace companion::visual_query::features
