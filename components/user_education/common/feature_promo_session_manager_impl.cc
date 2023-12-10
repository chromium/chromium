// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_session_manager_impl.h"

#include "ui/base/idle/idle.h"

namespace user_education {

PollingIdleObserver::PollingIdleObserver() = default;
PollingIdleObserver::~PollingIdleObserver() = default;

FeaturePromoSessionManager::IdleState PollingIdleObserver::GetCurrentState()
    const {
  return FeaturePromoSessionManager::IdleState{
      GetCurrentTime() - base::Seconds(ui::CalculateIdleTime()),
      ui::CheckIdleStateIsLocked()};
}

void PollingIdleObserver::StartObserving() {
  service_observer_.Observe(ui::IdlePollingService::GetInstance());
}

void PollingIdleObserver::OnIdleStateChange(
    const ui::IdlePollingService::State& state) {
  NotifyIdleStateChanged(FeaturePromoSessionManager::IdleState{
      GetCurrentTime() - state.idle_time, state.locked});
}

}  // namespace user_education
