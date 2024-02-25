// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_session_manager.h"

#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_idle_policy.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/user_education_features.h"

namespace user_education {

FeaturePromoSessionManager::FeaturePromoSessionManager() = default;
FeaturePromoSessionManager::~FeaturePromoSessionManager() = default;

void FeaturePromoSessionManager::Init(
    FeaturePromoStorageService* storage_service,
    std::unique_ptr<FeaturePromoIdleObserver> idle_observer,
    std::unique_ptr<FeaturePromoIdlePolicy> idle_policy) {
  storage_service_ = storage_service;
  idle_observer_ = std::move(idle_observer);
  idle_policy_ = std::move(idle_policy);

  idle_observer_->Init(storage_service_.get());
  idle_policy_->Init(this, storage_service_.get());

  // Immediately update the current state, then subscribe to future updates.
  UpdateIdleState(idle_observer_->GetCurrentState());
  idle_observer_subscription_ = idle_observer_->AddUpdateCallback(
      base::BindRepeating(&FeaturePromoSessionManager::UpdateIdleState,
                          base::Unretained(this)));
  idle_observer_->StartObserving();
}

bool FeaturePromoSessionManager::IsApplicationActive() const {
  return idle_policy_ && application_is_active_ &&
         idle_policy_->IsActive(
             storage_service_->ReadSessionData().most_recent_active_time);
}

void FeaturePromoSessionManager::OnNewSession(
    const base::Time old_start_time,
    const base::Time old_active_time,
    const base::Time new_active_time) {
  base::RecordAction(
      base::UserMetricsAction("UserEducation.Session.ActivePeriodStart"));

  // Now starting new session. The Active Period of the old session
  // is the difference between old session times.
  RecordActivePeriodDuration(old_active_time - old_start_time);

  // The now-elapsed Idle Period is difference between now and the
  // previous most_recent_active_time.
  RecordIdlePeriodDuration(new_active_time - old_active_time);
}

void FeaturePromoSessionManager::OnIdleStateUpdating(const IdleState&) {}

void FeaturePromoSessionManager::RecordActivePeriodDuration(
    base::TimeDelta duration) {
  // Increments of 1 minute under 1 hour.
  base::UmaHistogramCustomCounts(
      "UserEducation.Session.ActivePeriodDuration.Min.Under1Hour",
      duration.InMinutes(), /*min=*/1,
      /*exclusive_max=*/60,
      /*buckets=*/60);

  // Increments of 15 minutes under 24 hours.
  base::UmaHistogramCustomCounts(
      "UserEducation.Session.ActivePeriodDuration.Min.Under24Hours",
      duration.InMinutes(), /*min=*/1,
      /*exclusive_max=*/60 * 24 /* minutes per 24 hours */,
      /*buckets=*/24 * 4 /* per 15 minutes */);
}

void FeaturePromoSessionManager::RecordIdlePeriodDuration(
    base::TimeDelta duration) {
  // Increments of 15 minutes under 24 hours.
  base::UmaHistogramCustomCounts(
      "UserEducation.Session.IdlePeriodDuration.Min.Under24Hours",
      duration.InMinutes(), /*min=*/1,
      /*exclusive_max=*/60 * 24 /* minutes per 24 hours */,
      /*buckets=*/24 * 4 /* per 15 minute */);

  // Increments of ~13 hours under 28 days.
  base::UmaHistogramCustomCounts(
      "UserEducation.Session.IdlePeriodDuration.Hr.Under28Days",
      duration.InHours(), /*min=*/1,
      /*exclusive_max=*/24 * 28 /* hours per 28 days */,
      /*buckets=*/50);
}

void FeaturePromoSessionManager::UpdateIdleState(
    const IdleState& new_idle_state) {
  CHECK(idle_policy_);
  OnIdleStateUpdating(new_idle_state);

  application_is_active_ = new_idle_state.application_active;
  if (!application_is_active_) {
    // While the machine may be active, the browser is not, or the screen is
    // locked, or some other thing that means the user isn't interacting with
    // the browser, so do not update the last active time.
    return;
  }

  if (!idle_policy_->IsActive(new_idle_state.last_active_time)) {
    // The machine is not active. Therefore, avoid updating the most recent
    // application active time; this time can be updated when the machine
    // becomes active again (possibly triggering a new session).
    return;
  }

  auto session_data = storage_service_->ReadSessionData();
  const auto old_start_time = session_data.start_time;
  const auto old_active_time = session_data.most_recent_active_time;
  const auto new_active_time = new_idle_state.last_active_time;
  session_data.most_recent_active_time = new_active_time;
  if (idle_policy_->IsNewSession(old_start_time, old_active_time,
                                 new_active_time)) {
    session_data.start_time = new_active_time;
    OnNewSession(old_start_time, old_active_time, new_active_time);
  }
  storage_service_->SaveSessionData(session_data);
}

}  // namespace user_education
