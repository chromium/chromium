// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_idle_policy.h"

#include "base/time/time.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/user_education_features.h"

namespace user_education {

// static
base::TimeDelta FeaturePromoIdlePolicy::idle_time_resolution_;
constexpr double FeaturePromoIdlePolicy::kIdleTimeResolutionFactor;

FeaturePromoIdlePolicy::FeaturePromoIdlePolicy()
    : FeaturePromoIdlePolicy(features::GetTimeToIdle(),
                             features::GetIdleTimeBetweenSessions(),
                             features::GetMinimumValidSessionLength()) {}

FeaturePromoIdlePolicy::FeaturePromoIdlePolicy(
    base::TimeDelta minimum_idle_time,
    base::TimeDelta new_session_idle_time,
    base::TimeDelta minimum_valid_session_length)
    : minimum_idle_time_(minimum_idle_time),
      new_session_idle_time_(new_session_idle_time),
      minimum_valid_session_length_(minimum_valid_session_length) {
  DCHECK(minimum_idle_time.is_positive());
  DCHECK_GT(new_session_idle_time, minimum_idle_time);
  DCHECK(!minimum_valid_session_length.is_negative());
}

FeaturePromoIdlePolicy::~FeaturePromoIdlePolicy() = default;

void FeaturePromoIdlePolicy::Init(
    const FeaturePromoSessionManager* session_manager,
    const FeaturePromoStorageService* storage_service) {
  session_manager_ = session_manager;
  storage_service_ = storage_service;
}

bool FeaturePromoIdlePolicy::IsActive(
    base::Time most_recent_active_time) const {
  const auto inactive_time =
      storage_service_->GetCurrentTime() - most_recent_active_time;
  return inactive_time < minimum_idle_time();
}

bool FeaturePromoIdlePolicy::IsNewSession(
    base::Time previous_session_start_time,
    base::Time previous_last_active_time,
    base::Time most_recent_active_time) const {
  const auto last_session_length =
      most_recent_active_time - previous_session_start_time;
  const auto time_between_active =
      most_recent_active_time - previous_last_active_time;
  return time_between_active >= new_session_idle_time() &&
         last_session_length >= minimum_valid_session_length();
}

// static
void FeaturePromoIdlePolicy::SetIdleTimeResolution(base::TimeDelta resolution) {
  if (idle_time_resolution_.is_zero()) {
    idle_time_resolution_ = resolution;
    DUMP_WILL_BE_CHECK_GE(features::GetTimeToIdle(),
                          kIdleTimeResolutionFactor * resolution)
        << "Time to idle (" << features::GetTimeToIdle()
        << ") is too short to resolve; resolution is " << resolution;
  } else {
    CHECK_EQ(idle_time_resolution_, resolution);
  }
}

}  // namespace user_education
