// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_feature_configs.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace omnibox_feature_configs {

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
}
// static
const ShortcutBoosting& ShortcutBoosting::Get() {
  static ShortcutBoosting config;
  return config;
}

}  // namespace omnibox_feature_configs
