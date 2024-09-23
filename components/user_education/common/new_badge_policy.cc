// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/new_badge_policy.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/user_education_features.h"

namespace user_education {

NewBadgePolicy::NewBadgePolicy()
    : NewBadgePolicy(features::GetNewBadgeShowCount(),
                     features::GetNewBadgeFeatureUsedCount(),
                     features::GetNewBadgeDisplayWindow(),
                     features::GetNewProfileGracePeriod()) {}

NewBadgePolicy::NewBadgePolicy(int times_shown_before_dismiss,
                               int uses_before_dismiss,
                               base::TimeDelta display_window,
                               base::TimeDelta new_profile_grace_period)
    : times_shown_before_dismiss_(times_shown_before_dismiss),
      uses_before_dismiss_(uses_before_dismiss),
      display_window_(display_window),
      new_profile_grace_period_(new_profile_grace_period) {}

NewBadgePolicy::~NewBadgePolicy() = default;

bool NewBadgePolicy::ShouldShowNewBadge(
    const NewBadgeData& data,
    const FeaturePromoStorageService& storage_service) const {
  // Check counts and window.
  if (data.show_count >= std::max(1, times_shown_before_dismiss_) ||
      data.used_count >= std::max(1, uses_before_dismiss_) ||
      storage_service.GetCurrentTime() >
          data.feature_enabled_time + display_window_) {
    return false;
  }

  // If the feature was rolled out during the grace period, the user has
  // basically always had it, so it's not new.
  if (data.feature_enabled_time <
      storage_service.profile_creation_time() + new_profile_grace_period_) {
    return false;
  }

  return true;
}

void NewBadgePolicy::RecordNewBadgeShown(const base::Feature& feature,
                                         int count) {
  base::UmaHistogramBoolean(base::StrCat({"UserEducation.NewBadge.",
                                          feature.name, ".MaxShownReached"}),
                            count >= times_shown_before_dismiss_);
}

void NewBadgePolicy::RecordFeatureUsed(const base::Feature& feature,
                                       int count) {
  // Only record histograms up to the max count. The value of the histogram
  // becomes true when the count hits max and the badge becomes disabled.
  if (count <= uses_before_dismiss_) {
    base::UmaHistogramBoolean(base::StrCat({"UserEducation.NewBadge.",
                                            feature.name, ".MaxUsedReached"}),
                              count == uses_before_dismiss_);
  }
}

int NewBadgePolicy::GetFeatureUsedStorageCap() const {
  // Always record more uses than the cap by a significant margin so that if
  // parameters change, there is still enough data to interpret the rule.
  return std::max(uses_before_dismiss_ * 2, uses_before_dismiss_ + 5);
}

}  // namespace user_education
