// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_SESSION_MOCKS_H_
#define COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_SESSION_MOCKS_H_

#include "components/user_education/common/feature_promo_idle_observer.h"
#include "components/user_education/common/feature_promo_idle_policy.h"
#include "components/user_education/common/feature_promo_session_manager.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace user_education::test {

// Version of `IdleObserver` that returns a provided state and which sends state
// updates only when `UpdateState()` is called.
class TestIdleObserver : public FeaturePromoIdleObserver {
 public:
  explicit TestIdleObserver(std::optional<base::Time> initial_active_time);
  ~TestIdleObserver() override;

  // Call to modify the current state and optionally send an update.
  void SetLastActiveTime(std::optional<base::Time> new_last_active_time,
                         bool send_update);

 private:
  // FeaturePromoIdleObserver:
  void StartObserving() final;
  std::optional<base::Time> MaybeGetNewLastActiveTime() const final;

  std::optional<base::Time> last_active_time_;
};

// Mock version of `IdlePolicy` that allows specific queries to be intercepted.
class MockIdlePolicy : public FeaturePromoIdlePolicy {
 public:
  MockIdlePolicy();
  ~MockIdlePolicy() override;

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
  MOCK_METHOD(void, OnLastActiveTimeUpdating, (base::Time), (override));
  MOCK_METHOD(void,
              OnNewSession,
              (const base::Time, const base::Time, const base::Time),
              (override));
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_FEATURE_PROMO_SESSION_MOCKS_H_
