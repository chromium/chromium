// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_session_manager.h"
#include <utility>

#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_idle_observer.h"
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
  idle_policy_ = std::move(idle_policy);
  idle_policy_->Init(this, storage_service_.get());
  // Assume the application is active at application start; this avoids making
  // additional system calls during startup.
  UpdateLastActiveTime(storage_service_->GetCurrentTime());
  // Start observing state.
  SetIdleObserver(std::move(idle_observer));
}

void FeaturePromoSessionManager::MaybeUpdateSessionState() {
  if (!storage_service_) {
    return;
  }

  // Determine if a new session could be started.
  const auto old_state = storage_service_->ReadSessionData();
  const auto now = storage_service_->GetCurrentTime();
  if (!idle_policy_->IsNewSession(old_state.start_time,
                                  old_state.most_recent_active_time, now)) {
    return;
  }

  const auto last_active = idle_observer_->MaybeGetNewLastActiveTime();
  if (last_active) {
    UpdateLastActiveTime(*last_active);
  }
}

base::CallbackListSubscription
FeaturePromoSessionManager::AddNewSessionCallback(
    base::RepeatingClosure new_session_callback) {
  return new_session_callbacks_.Add(std::move(new_session_callback));
}

void FeaturePromoSessionManager::OnNewSession(
    const base::Time old_start_time,
    const base::Time old_active_time,
    const base::Time new_active_time) {
  new_session_since_startup_ = true;

  base::RecordAction(
      base::UserMetricsAction("UserEducation.Session.ActivePeriodStart"));

  // Now starting new session. The Active Period of the old session
  // is the difference between old session times.
  RecordActivePeriodDuration(old_active_time - old_start_time);

  // The now-elapsed Idle Period is difference between now and the
  // previous most_recent_active_time.
  RecordIdlePeriodDuration(new_active_time - old_active_time);

  // Notify any listeners of the new session.
  new_session_callbacks_.Notify();
}

void FeaturePromoSessionManager::OnLastActiveTimeUpdating(base::Time) {}

void FeaturePromoSessionManager::SetIdleObserver(
    std::unique_ptr<FeaturePromoIdleObserver> new_observer) {
  idle_observer_ = std::move(new_observer);
  idle_observer_->Init(storage_service_.get());
  idle_observer_subscription_ = idle_observer_->AddUpdateCallback(
      base::BindRepeating(&FeaturePromoSessionManager::UpdateLastActiveTime,
                          base::Unretained(this)));
  idle_observer_->StartObserving();
}

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

void FeaturePromoSessionManager::UpdateLastActiveTime(
    base::Time new_active_time) {
  CHECK(idle_policy_);
  OnLastActiveTimeUpdating(new_active_time);

  auto session_data = storage_service_->ReadSessionData();
  const auto old_start_time = session_data.start_time;
  const auto old_active_time = session_data.most_recent_active_time;
  session_data.most_recent_active_time = new_active_time;
  const bool is_new_session = idle_policy_->IsNewSession(
      old_start_time, old_active_time, new_active_time);
  if (is_new_session) {
    session_data.start_time = new_active_time;
  }
  // Save the session data before calling OnNewSession, since some listeners
  // will be relying on the data being current.
  storage_service_->SaveSessionData(session_data);
  if (is_new_session) {
    OnNewSession(old_start_time, old_active_time, new_active_time);
  }
}

}  // namespace user_education
