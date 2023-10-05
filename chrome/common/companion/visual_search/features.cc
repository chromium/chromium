// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/companion/visual_search/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace companion::visual_search::features {

BASE_FEATURE(kVisualSearchSuggestions,
             "VisualSearchSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVisualSearchSuggestionsAgent,
             "VisualSearchSuggestionsAgent",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsVisualSearchSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kVisualSearchSuggestions);
}

bool IsVisualSearchSuggestionsAgentEnabled() {
  return base::FeatureList::IsEnabled(kVisualSearchSuggestionsAgent);
}

base::TimeDelta StartClassificationRetryDuration() {
  return base::Milliseconds(GetFieldTrialParamByFeatureAsInt(
      kVisualSearchSuggestions, "classification_retry_delay_ms", 2000));
}

int MaxVisualSuggestions() {
  return GetFieldTrialParamByFeatureAsInt(kVisualSearchSuggestions,
                                          "max_visual_suggestions", 1);
}

}  // namespace companion::visual_search::features
