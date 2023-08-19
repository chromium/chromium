// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_feature_configs.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace omnibox_feature_configs {

// static
BASE_FEATURE(CalcProvider::kCalcProvider,
             "OmniboxCalcProvider",
             base::FEATURE_DISABLED_BY_DEFAULT);
CalcProvider::CalcProvider() {
  enabled = base::FeatureList::IsEnabled(kCalcProvider);
  score =
      base::FeatureParam<int>(&kCalcProvider, "CalcProviderScore", 900).Get();
  max_matches =
      base::FeatureParam<int>(&kCalcProvider, "CalcProviderMaxMatches", 5)
          .Get();
  num_non_calc_inputs =
      base::FeatureParam<int>(&kCalcProvider, "CalcProviderNumNonCalcInputs", 3)
          .Get();
}

// static
BASE_FEATURE(ShortcutBoosting::kShortcutBoost,
             "OmniboxShortcutBoost",
             base::FEATURE_DISABLED_BY_DEFAULT);
ShortcutBoosting::ShortcutBoosting() {
  enabled = base::FeatureList::IsEnabled(kShortcutBoost);
  search_score =
      base::FeatureParam<int>(&kShortcutBoost, "ShortcutBoostSearchScore", 0)
          .Get();
  url_score =
      base::FeatureParam<int>(&kShortcutBoost, "ShortcutBoostUrlScore", 0)
          .Get();
  counterfactual = base::FeatureParam<bool>(
                       &kShortcutBoost, "ShortcutBoostCounterfactual", false)
                       .Get();
  non_top_hit_threshold =
      base::FeatureParam<int>(&kShortcutBoost,
                              "ShortcutBoostNonTopHitThreshold", 0)
          .Get();
  group_with_searches =
      base::FeatureParam<bool>(&kShortcutBoost,
                               "ShortcutBoostGroupWithSearches", false)
          .Get();
}

}  // namespace omnibox_feature_configs
