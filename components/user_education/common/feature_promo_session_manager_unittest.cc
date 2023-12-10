// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_session_manager.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/test/mock_feature_promo_session_manager.h"
#include "components/user_education/test/test_feature_promo_storage_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_education {

namespace {
// Start in late 2022.
constexpr auto kSessionStartTime =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(365 * 422));
constexpr auto kPreviousActiveTime = kSessionStartTime + base::Minutes(30);
constexpr auto kNewActiveTime = kSessionStartTime + base::Minutes(62);
constexpr auto kNow = kSessionStartTime + base::Minutes(65);
constexpr auto kSecondNewActiveTime = kSessionStartTime + base::Minutes(66);
constexpr auto kNow2 = kSessionStartTime + base::Minutes(71);
constexpr FeaturePromoSessionManager::IdleState kInitialState{kNewActiveTime,
                                                              false};
}  // namespace

class FeaturePromoSessionManagerTest : public testing::Test {
 public:
  FeaturePromoSessionManagerTest() {
    clock_.SetNow(kNow);
    FeaturePromoSessionData previous_data;
    previous_data.start_time = kSessionStartTime;
    previous_data.most_recent_active_time = kPreviousActiveTime;
    storage_service_.SaveSessionData(previous_data);
    storage_service_.set_clock_for_testing(&clock_);
  }

  test::MockIdlePolicy& mock_idle_policy() {
    CHECK(mock_idle_policy_);
    return *mock_idle_policy_;
  }

  test::TestIdleObserver& idle_observer() {
    CHECK(idle_observer_);
    return *idle_observer_;
  }

  test::MockFeaturePromoSessionManager& session_manager() {
    return session_manager_;
  }

  void InitWithMockPolicy(bool new_session) {
    CHECK(!mock_idle_policy_);
    auto policy_ptr =
        std::make_unique<testing::StrictMock<test::MockIdlePolicy>>();
    mock_idle_policy_ = policy_ptr.get();

    EXPECT_CALL(mock_idle_policy(), IsActive(kNewActiveTime, false))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(
        mock_idle_policy(),
        IsNewSession(kSessionStartTime, kPreviousActiveTime, kNewActiveTime))
        .WillOnce(testing::Return(new_session));
    EXPECT_CALL(session_manager(), OnIdleStateUpdating(kNewActiveTime, false));
    if (new_session) {
      EXPECT_CALL(session_manager(), OnNewSession);
    }
    session_manager_.Init(&storage_service_, CreateIdleObserver(),
                          std::move(policy_ptr));
  }

  void InitWithTestPolicy(base::TimeDelta minimum_idle_time,
                          base::TimeDelta new_session_idle_time,
                          base::TimeDelta minimum_valid_session_length,
                          bool expect_new_session) {
    EXPECT_CALL(session_manager(), OnIdleStateUpdating(kNewActiveTime, false));
    if (expect_new_session) {
      EXPECT_CALL(session_manager(), OnNewSession);
    }
    session_manager_.Init(&storage_service_, CreateIdleObserver(),
                          std::make_unique<test::TestIdlePolicy>(
                              minimum_idle_time, new_session_idle_time,
                              minimum_valid_session_length));
  }

  base::Time GetSessionStartTime() const {
    return storage_service_.ReadSessionData().start_time;
  }

 protected:
  base::SimpleTestClock clock_;
  test::TestFeaturePromoStorageService storage_service_;
  testing::StrictMock<test::MockFeaturePromoSessionManager> session_manager_;

 private:
  std::unique_ptr<test::TestIdleObserver> CreateIdleObserver() {
    CHECK(!idle_observer_);
    auto ptr = std::make_unique<test::TestIdleObserver>(kInitialState);
    idle_observer_ = ptr.get();
    return ptr;
  }

  raw_ptr<test::TestIdleObserver> idle_observer_ = nullptr;
  raw_ptr<test::MockIdlePolicy> mock_idle_policy_ = nullptr;
};

TEST_F(FeaturePromoSessionManagerTest, CreateVanillaSessionManager) {
  auto observer_ptr =
      std::make_unique<FeaturePromoSessionManager::IdleObserver>();
  auto policy_ptr = std::make_unique<FeaturePromoSessionManager::IdlePolicy>();
  FeaturePromoSessionManager manager;
  manager.Init(&storage_service_, std::move(observer_ptr),
               std::move(policy_ptr));
  // Last active time was over half an hour ago.
  EXPECT_FALSE(manager.IsApplicationActive());
  EXPECT_EQ(kSessionStartTime, GetSessionStartTime());
}

TEST_F(FeaturePromoSessionManagerTest, CheckIdlePolicyDefaults) {

  auto observer_ptr = std::make_unique<test::TestIdleObserver>(kInitialState);
  auto policy_ptr = std::make_unique<FeaturePromoSessionManager::IdlePolicy>();

  auto* const observer = observer_ptr.get();

  FeaturePromoSessionManager manager;
  manager.Init(&storage_service_, std::move(observer_ptr),
               std::move(policy_ptr));

  // Moving just a little bit later should not result in a new session.
  const auto kALittleLater = kNow + base::Milliseconds(500);
  const auto kALittleLaterNow = kALittleLater + base::Milliseconds(500);
  clock_.SetNow(kALittleLaterNow);
  observer->UpdateState({kALittleLater, false});
  EXPECT_TRUE(manager.IsApplicationActive());
  EXPECT_EQ(kSessionStartTime, GetSessionStartTime());

  // Moving to a much later time will result in a new session if everything is
  // configured properly.
  const auto kMuchLater = kNow + base::Days(5);
  const auto kMuchLaterNow = kMuchLater + base::Seconds(1);
  clock_.SetNow(kMuchLaterNow);
  observer->UpdateState({kMuchLater, false});
  EXPECT_TRUE(manager.IsApplicationActive());
  EXPECT_EQ(kMuchLater, GetSessionStartTime());
}

TEST_F(FeaturePromoSessionManagerTest, InitOnIdleStateUpdating) {
  InitWithMockPolicy(false);
}

TEST_F(FeaturePromoSessionManagerTest, InitOnNewSession) {
  InitWithMockPolicy(true);
}

TEST_F(FeaturePromoSessionManagerTest, IdleUpdatedNotActiveNoNewSession) {
  InitWithMockPolicy(false);
  EXPECT_CALL(mock_idle_policy(), IsActive(kSecondNewActiveTime, false))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(session_manager(),
              OnIdleStateUpdating(kSecondNewActiveTime, false));
  clock_.SetNow(kNow2);
  idle_observer().UpdateState({kSecondNewActiveTime, false});
}

TEST_F(FeaturePromoSessionManagerTest, IdleUpdatedLockedNotActiveNoNewSession) {
  InitWithMockPolicy(false);
  EXPECT_CALL(mock_idle_policy(), IsActive(kSecondNewActiveTime, true))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(session_manager(),
              OnIdleStateUpdating(kSecondNewActiveTime, true));
  clock_.SetNow(kNow2);
  idle_observer().UpdateState({kSecondNewActiveTime, true});
}

TEST_F(FeaturePromoSessionManagerTest, IdleUpdatedActiveNoNewSession) {
  InitWithMockPolicy(false);
  EXPECT_CALL(mock_idle_policy(), IsActive(kSecondNewActiveTime, false))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(
      mock_idle_policy(),
      IsNewSession(kSessionStartTime, kNewActiveTime, kSecondNewActiveTime))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(session_manager(),
              OnIdleStateUpdating(kSecondNewActiveTime, false));
  clock_.SetNow(kNow2);
  idle_observer().UpdateState({kSecondNewActiveTime, false});
}

TEST_F(FeaturePromoSessionManagerTest, IdleUpdatedActiveNewSession) {
  InitWithMockPolicy(false);
  EXPECT_CALL(mock_idle_policy(), IsActive(kSecondNewActiveTime, false))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(
      mock_idle_policy(),
      IsNewSession(kSessionStartTime, kNewActiveTime, kSecondNewActiveTime))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(session_manager(),
              OnIdleStateUpdating(kSecondNewActiveTime, false));
  EXPECT_CALL(session_manager(), OnNewSession);
  clock_.SetNow(kNow2);
  idle_observer().UpdateState({kSecondNewActiveTime, false});
}

TEST_F(FeaturePromoSessionManagerTest, NoNewSessionFromIdle) {
  // Idle gap in test data is 30 minutes, so 60 minute idle time means no new
  // session.
  InitWithTestPolicy(base::Minutes(5), base::Minutes(60), base::Minutes(30),
                     false);
}

TEST_F(FeaturePromoSessionManagerTest, NewSessionFromIdle) {
  // Idle gap in test data is 30 minutes, so 30 minute idle time means a new
  // session.
  InitWithTestPolicy(base::Minutes(5), base::Minutes(30), base::Minutes(30),
                     true);
}

TEST_F(FeaturePromoSessionManagerTest,
       NoNewSessionFromIdleDueToMinimumSessionTime) {
  // Idle gap in test data is 30 minutes, so 30 minute idle time would mean a
  // new session, except that total session length is only 60 minutes.
  InitWithTestPolicy(base::Minutes(5), base::Minutes(30), base::Minutes(70),
                     false);
}

TEST_F(FeaturePromoSessionManagerTest, IsApplicationActiveIdleTime) {
  InitWithTestPolicy(base::Minutes(10), base::Minutes(30), base::Minutes(70),
                     false);
  EXPECT_TRUE(session_manager().IsApplicationActive());
  clock_.SetNow(kNewActiveTime + base::Minutes(5));
  EXPECT_TRUE(session_manager().IsApplicationActive());
  clock_.SetNow(kNewActiveTime + base::Minutes(10));
  EXPECT_FALSE(session_manager().IsApplicationActive());
  clock_.SetNow(kNewActiveTime + base::Minutes(15));
  EXPECT_FALSE(session_manager().IsApplicationActive());
}

TEST_F(FeaturePromoSessionManagerTest, IsApplicationActiveIdleUpdate) {
  InitWithTestPolicy(base::Minutes(4), base::Minutes(30), base::Minutes(70),
                     false);
  // The second idle update has an idle time larger than our threshold (4) so
  // this represents an idle application.
  EXPECT_CALL(session_manager(),
              OnIdleStateUpdating(kSecondNewActiveTime, false));
  clock_.SetNow(kNow2);
  idle_observer().UpdateState({kSecondNewActiveTime, false});
  EXPECT_FALSE(session_manager().IsApplicationActive());
}

TEST_F(FeaturePromoSessionManagerTest, IsApplicationActiveComputerLocked) {
  InitWithTestPolicy(base::Minutes(10), base::Minutes(30), base::Minutes(70),
                     false);
  // This is within the idle window but the computer is locked, so the
  // application isn't active.
  EXPECT_CALL(session_manager(),
              OnIdleStateUpdating(kSecondNewActiveTime, true));
  clock_.SetNow(kNow2);
  idle_observer().UpdateState({kSecondNewActiveTime, true});
  EXPECT_FALSE(session_manager().IsApplicationActive());
}

}  // namespace user_education
