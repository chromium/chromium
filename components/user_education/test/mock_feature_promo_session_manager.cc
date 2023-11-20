// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/mock_feature_promo_session_manager.h"

namespace user_education::test {

TestIdleObserver::TestIdleObserver(
    FeaturePromoSessionManager::IdleState initial_state)
    : state_(initial_state) {}
TestIdleObserver::~TestIdleObserver() = default;

void TestIdleObserver::UpdateState(
    const FeaturePromoSessionManager::IdleState& new_state) {
  state_ = new_state;
  NotifyIdleStateChanged(state_);
}

void TestIdleObserver::StartObserving() {}

FeaturePromoSessionManager::IdleState TestIdleObserver::GetCurrentState()
    const {
  return state_;
}

TestIdlePolicy::TestIdlePolicy(base::TimeDelta minimum_idle_time,
                               base::TimeDelta new_session_idle_time,
                               base::TimeDelta minimum_valid_session_length)
    : IdlePolicy(minimum_idle_time,
                 new_session_idle_time,
                 minimum_valid_session_length) {}
TestIdlePolicy::~TestIdlePolicy() = default;

MockIdlePolicy::MockIdlePolicy() = default;
MockIdlePolicy::~MockIdlePolicy() = default;

MockFeaturePromoSessionManager::MockFeaturePromoSessionManager() = default;
MockFeaturePromoSessionManager::~MockFeaturePromoSessionManager() = default;

}  // namespace user_education::test
