// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/new_badge_policy.h"
#include <algorithm>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/user_education_features.h"

namespace user_education {

NewBadgePolicy::NewBadgePolicy()
    : NewBadgePolicy(features::GetNewBadgeShowCount(),
                     features::GetNewBadgeFeatureUsedCount(),
                     features::GetNewBadgeDisplayWindow()) {}

NewBadgePolicy::NewBadgePolicy(int times_shown_before_dismiss,
                               int uses_before_dismiss,
                               base::TimeDelta display_window)
    : times_shown_before_dismiss_(times_shown_before_dismiss),
      uses_before_dismiss_(uses_before_dismiss),
      display_window_(display_window) {}

NewBadgePolicy::~NewBadgePolicy() = default;

bool NewBadgePolicy::ShouldShowNewBadge(
    const base::Feature& feature,
    int show_count,
    int used_count,
    base::TimeDelta time_since_enabled) const {
  if (show_count >= std::max(1, times_shown_before_dismiss_) ||
      used_count >= std::max(1, uses_before_dismiss_) ||
      time_since_enabled > display_window_) {
    return false;
  }

  return true;
}

}  // namespace user_education
