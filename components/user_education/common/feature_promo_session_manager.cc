// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_session_manager.h"

#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/user_education_features.h"

namespace user_education {

// Monitors the idle state of the current program/computer using various low-
// level APIs.
FeaturePromoSessionManager::IdleObserver::IdleObserver() = default;
FeaturePromoSessionManager::IdleObserver::~IdleObserver() = default;

void FeaturePromoSessionManager::IdleObserver::StartObserving() {}

FeaturePromoSessionManager::IdleState
FeaturePromoSessionManager::IdleObserver::GetCurrentState() const {
  return IdleState();
}

base::CallbackListSubscription
FeaturePromoSessionManager::IdleObserver::AddUpdateCallback(
    UpdateCallback update_callback) {
  return update_callbacks_.Add(std::move(update_callback));
}

void FeaturePromoSessionManager::IdleObserver::NotifyIdleStateChanged(
    const IdleState& state) {
  update_callbacks_.Notify(state);
}

base::Time FeaturePromoSessionManager::IdleObserver::GetCurrentTime() const {
  return session_manager_->storage_service_->GetCurrentTime();
}

FeaturePromoSessionManager::IdlePolicy::IdlePolicy()
    : IdlePolicy(features::GetTimeToIdle(),
                 features::GetIdleTimeBetweenSessions(),
                 features::GetMinimumValidSessionLength()) {}

FeaturePromoSessionManager::IdlePolicy::IdlePolicy(
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

FeaturePromoSessionManager::IdlePolicy::~IdlePolicy() = default;

bool FeaturePromoSessionManager::IdlePolicy::IsActive(
    base::Time most_recent_active_time,
    bool is_locked) const {
  const auto inactive_time =
      session_manager_->storage_service_->GetCurrentTime() -
      most_recent_active_time;
  return !is_locked && inactive_time < minimum_idle_time();
}

bool FeaturePromoSessionManager::IdlePolicy::IsNewSession(
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

FeaturePromoSessionManager::FeaturePromoSessionManager() = default;
FeaturePromoSessionManager::~FeaturePromoSessionManager() = default;

void FeaturePromoSessionManager::Init(
    FeaturePromoStorageService* storage_service,
    std::unique_ptr<IdleObserver> idle_observer,
    std::unique_ptr<IdlePolicy> idle_policy) {
  storage_service_ = storage_service;
  idle_observer_ = std::move(idle_observer);
  idle_policy_ = std::move(idle_policy);

  idle_observer_->session_manager_ = this;
  idle_policy_->session_manager_ = this;

  // Immediately update the current state, then subscribe to future updates.
  UpdateIdleState(idle_observer_->GetCurrentState());
  idle_observer_subscription_ = idle_observer_->AddUpdateCallback(
      base::BindRepeating(&FeaturePromoSessionManager::UpdateIdleState,
                          base::Unretained(this)));
  idle_observer_->StartObserving();
}

bool FeaturePromoSessionManager::IsApplicationActive() const {
  return idle_policy_ &&
         idle_policy_->IsActive(
             storage_service_->ReadSessionData().most_recent_active_time,
             is_locked_);
}

void FeaturePromoSessionManager::OnNewSession() {}

void FeaturePromoSessionManager::OnIdleStateUpdating(
    base::Time new_last_active_time,
    bool new_locked_state) {}

void FeaturePromoSessionManager::UpdateIdleState(
    const IdleState& new_idle_state) {
  CHECK(idle_policy_);
  OnIdleStateUpdating(new_idle_state.last_active_time,
                      new_idle_state.screen_locked);

  is_locked_ = new_idle_state.screen_locked;

  auto session_data = storage_service_->ReadSessionData();
  const auto old_start_time = session_data.start_time;
  const auto old_active_time = session_data.most_recent_active_time;
  const auto new_active_time = new_idle_state.last_active_time;

  session_data.most_recent_active_time = new_active_time;
  if (idle_policy_->IsActive(new_active_time, is_locked_)) {
    if (idle_policy_->IsNewSession(old_start_time, old_active_time,
                                   new_active_time)) {
      session_data.start_time = new_active_time;
      OnNewSession();
    }
  }
  storage_service_->SaveSessionData(session_data);
}

}  // namespace user_education
