// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/features.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"

#include <algorithm>
#include <map>

namespace gcm {

namespace features {

BASE_FEATURE(kInvalidateTokenFeature,
             "GCMTokenInvalidAfterDays",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kParamNameTokenInvalidationPeriodDays[] =
    "token_invalidation_period";
// A token invalidation period of 0 means the feature is disabled, and the
// GCM token never becomes stale.
const int kDefaultTokenInvalidationPeriod = 7;

base::TimeDelta GetTokenInvalidationInterval() {
  if (!base::FeatureList::IsEnabled(kInvalidateTokenFeature))
    return base::TimeDelta();
  std::string override_value = base::GetFieldTrialParamValueByFeature(
      kInvalidateTokenFeature, kParamNameTokenInvalidationPeriodDays);

  if (!override_value.empty()) {
    int override_value_days;
    if (base::StringToInt(override_value, &override_value_days))
      return base::Days(override_value_days);
  }
  return base::Days(kDefaultTokenInvalidationPeriod);
}

}  // namespace features

}  // namespace gcm
