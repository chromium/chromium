// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace search_features {

const base::Feature kQuerySearchBurnInPeriod{"QuerySearchBurnInPeriod",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

bool IsQuerySearchBurnInPeriodEnabled() {
  return base::FeatureList::IsEnabled(kQuerySearchBurnInPeriod);
}

base::TimeDelta QuerySearchBurnInPeriodDuration() {
  int ms = base::GetFieldTrialParamByFeatureAsInt(
      kQuerySearchBurnInPeriod, "burnin_period", /*default_value */ 100);
  return base::TimeDelta(base::Milliseconds(ms));
}

}  // namespace search_features
