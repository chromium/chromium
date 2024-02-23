// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_idle_observer.h"
#include "components/user_education/common/feature_promo_storage_service.h"

namespace user_education {

// Monitors the idle state of the current program/computer using various low-
// level APIs.
FeaturePromoIdleObserver::FeaturePromoIdleObserver() = default;
FeaturePromoIdleObserver::~FeaturePromoIdleObserver() = default;

void FeaturePromoIdleObserver::Init(
    const FeaturePromoStorageService* storage_service) {
  storage_service_ = storage_service;
}

void FeaturePromoIdleObserver::StartObserving() {}

FeaturePromoIdleObserver::IdleState FeaturePromoIdleObserver::GetCurrentState()
    const {
  return IdleState();
}

base::CallbackListSubscription FeaturePromoIdleObserver::AddUpdateCallback(
    UpdateCallback update_callback) {
  return update_callbacks_.Add(std::move(update_callback));
}

void FeaturePromoIdleObserver::NotifyIdleStateChanged(const IdleState& state) {
  update_callbacks_.Notify(state);
}

base::Time FeaturePromoIdleObserver::GetCurrentTime() const {
  return storage_service_->GetCurrentTime();
}

std::ostream& operator<<(
    std::ostream& os,
    const FeaturePromoIdleObserver::IdleState& idle_state) {
  os << "IdleState{ " << idle_state.last_active_time << ", "
     << idle_state.application_active << " }";
  return os;
}

}  // namespace user_education
