// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_MOCK_FEATURE_PROMO_SESSION_MANAGER_H_
#define COMPONENTS_USER_EDUCATION_TEST_MOCK_FEATURE_PROMO_SESSION_MANAGER_H_

#include "components/user_education/common/feature_promo_session_manager.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace user_education::test {

// Version of `IdleObserver` that returns a provided state and which sends state
// updates only when `UpdateState()` is called.
class TestIdleObserver : public FeaturePromoSessionManager::IdleObserver {
 public:
  explicit TestIdleObserver(
      FeaturePromoSessionManager::IdleState initial_state);
  ~TestIdleObserver() override;

  // Call to modify the current state and send an update.
  void UpdateState(const FeaturePromoSessionManager::IdleState& new_state);

 private:
  void StartObserving() final;
  FeaturePromoSessionManager::IdleState GetCurrentState() const final;

  FeaturePromoSessionManager::IdleState state_;
};

// Version of `IdlePolicy` that allows specific threshold values to be set.
class TestIdlePolicy : public FeaturePromoSessionManager::IdlePolicy {
 public:
  TestIdlePolicy(base::TimeDelta minimum_idle_time,
                 base::TimeDelta new_session_idle_time,
                 base::TimeDelta minimum_valid_session_length);
  ~TestIdlePolicy() override;
};

// Mock version of `IdlePolicy` that allows specific queries to be intercepted.
class MockIdlePolicy : public FeaturePromoSessionManager::IdlePolicy {
 public:
  MockIdlePolicy();
  ~MockIdlePolicy() override;

  MOCK_METHOD(bool, IsActive, (base::Time, bool), (const, override));
  MOCK_METHOD(bool,
              IsNewSession,
              (base::Time, base::Time, base::Time),
              (const, override));
};

// Mock version of `FeaturePromoSessionManager` that can monitor when updates or
// new sessions happen.
class MockFeaturePromoSessionManager : public FeaturePromoSessionManager {
 public:
  MockFeaturePromoSessionManager();
  ~MockFeaturePromoSessionManager() override;

  MOCK_METHOD(void, OnIdleStateUpdating, (base::Time, bool), (override));
  MOCK_METHOD(void, OnNewSession, (), (override));
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_MOCK_FEATURE_PROMO_SESSION_MANAGER_H_
