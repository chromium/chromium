// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_session_manager.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/test/mock_feature_promo_session_manager.h"
#include "components/user_education/test/test_feature_promo_storage_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_education {

// Base class that uses a test idle observer to test basic functionality of a
// FeaturePromoSessionManager.
class FeaturePromoSessionManagerTest : public testing::Test {
 public:
  FeaturePromoSessionManagerTest() = default;
  ~FeaturePromoSessionManagerTest() override = default;

  using IdleState = FeaturePromoSessionManager::IdleState;

  test::TestFeaturePromoStorageService& storage_service() {
    return storage_service_;
  }

  base::SimpleTestClock& clock() { return clock_; }

  void InitSession(base::Time session_start,
                   base::Time last_active,
                   base::Time now) {
    clock_.SetNow(now);
    FeaturePromoSessionData previous_data;
    previous_data.start_time = session_start;
    previous_data.most_recent_active_time = last_active;
    storage_service_.set_clock_for_testing(&clock_);
    storage_service_.SaveSessionData(previous_data);
    const auto new_data = storage_service_.ReadSessionData();
  }

  void CheckSessionData(base::Time expected_session_start,
                        base::Time expected_last_active) {
    const auto data = storage_service().ReadSessionData();
    EXPECT_EQ(expected_session_start, data.start_time);
    EXPECT_EQ(expected_last_active, data.most_recent_active_time);
  }

 private:
  base::SimpleTestClock clock_;
  test::TestFeaturePromoStorageService storage_service_;
};

TEST_F(FeaturePromoSessionManagerTest, CreateVanillaSessionManager) {
  const auto now = base::Time::Now();
  const auto last_active = now - base::Hours(2);
  const auto start_time = now - base::Hours(4);
  InitSession(start_time, last_active, now);
  auto observer_ptr =
      std::make_unique<FeaturePromoSessionManager::IdleObserver>();
  auto policy_ptr = std::make_unique<FeaturePromoSessionManager::IdlePolicy>();
  FeaturePromoSessionManager manager;
  manager.Init(&storage_service(), std::move(observer_ptr),
               std::move(policy_ptr));
  // Last active time was over half an hour ago.
  EXPECT_FALSE(manager.IsApplicationActive());
  EXPECT_EQ(start_time, storage_service().ReadSessionData().start_time);
}

TEST_F(FeaturePromoSessionManagerTest, CheckIdlePolicyDefaults) {
  // Start in the middle of a session, currently active.
  const auto now = base::Time::Now();
  const auto start_time = now - base::Hours(4);
  InitSession(start_time, now, now);

  auto observer_ptr = std::make_unique<test::TestIdleObserver>(IdleState{now});
  auto policy_ptr = std::make_unique<FeaturePromoSessionManager::IdlePolicy>();

  auto* const observer = observer_ptr.get();

  FeaturePromoSessionManager manager;
  manager.Init(&storage_service(), std::move(observer_ptr),
               std::move(policy_ptr));

  // Moving just a little bit later should not result in a new session.
  const auto kALittleLater = now + base::Milliseconds(500);
  const auto kALittleLaterNow = kALittleLater + base::Milliseconds(500);
  clock().SetNow(kALittleLaterNow);
  observer->UpdateState(IdleState{kALittleLater});
  EXPECT_TRUE(manager.IsApplicationActive());
  CheckSessionData(start_time, kALittleLater);

  // Moving to a much later time will result in a new session if everything is
  // configured properly.
  const auto kMuchLater = now + base::Days(5);
  const auto kMuchLaterNow = kMuchLater + base::Seconds(1);
  clock().SetNow(kMuchLaterNow);
  observer->UpdateState(IdleState{kMuchLater});
  EXPECT_TRUE(manager.IsApplicationActive());
  CheckSessionData(kMuchLater, kMuchLater);
}

// Base test for more advanced tests; provides the ability to simulate idle
// state updates and uses a test clock.
class FeaturePromoSessionManagerWithMockManagerTest
    : public FeaturePromoSessionManagerTest {
 public:
  FeaturePromoSessionManagerWithMockManagerTest() = default;
  ~FeaturePromoSessionManagerWithMockManagerTest() override = default;

  test::MockFeaturePromoSessionManager& session_manager() {
    return session_manager_;
  }

  void InitSessionManager(
      const IdleState& initial_state,
      std::unique_ptr<FeaturePromoSessionManager::IdlePolicy> idle_policy) {
    session_manager_.Init(&storage_service(), CreateIdleObserver(initial_state),
                          std::move(idle_policy));
  }

  // Moves the clock forward and updates the current idle state and verifies the
  // expected update calls.
  //
  // If `suppress_last_active_update` is true, does not expect the last active
  // time to be updated.
  void UpdateState(IdleState new_state,
                   base::Time new_now,
                   bool expect_new_session,
                   bool suppress_last_active_update = false) {
    EXPECT_CALL(session_manager(), OnIdleStateUpdating(new_state));
    const auto data = storage_service().ReadSessionData();
    if (expect_new_session) {
      CHECK(new_state.application_active);
      CHECK(!suppress_last_active_update);
      EXPECT_CALL(session_manager(),
                  OnNewSession(data.start_time, data.most_recent_active_time,
                               new_state.last_active_time));
    }
    clock().SetNow(new_now);
    idle_observer_->UpdateState(new_state);
    CheckSessionData(
        expect_new_session ? new_state.last_active_time : data.start_time,
        new_state.application_active && !suppress_last_active_update
            ? new_state.last_active_time
            : data.most_recent_active_time);
  }

 private:
  std::unique_ptr<test::TestIdleObserver> CreateIdleObserver(
      const IdleState& initial_state) {
    CHECK(!idle_observer_);
    auto ptr = std::make_unique<test::TestIdleObserver>(initial_state);
    idle_observer_ = ptr.get();
    return ptr;
  }

  testing::StrictMock<test::MockFeaturePromoSessionManager> session_manager_;
  raw_ptr<test::TestIdleObserver> idle_observer_ = nullptr;
};

// These tests ensure that the correct methods on IdlePolicy get called with the
// expected parameters.
class FeaturePromoSessionManagerWithMockPolicyTest
    : public FeaturePromoSessionManagerWithMockManagerTest {
 public:
  FeaturePromoSessionManagerWithMockPolicyTest() = default;
  ~FeaturePromoSessionManagerWithMockPolicyTest() override = default;

 protected:
  test::MockIdlePolicy& idle_policy() { return *idle_policy_; }

  void Init(base::Time session_start,
            base::Time last_active,
            IdleState new_state,
            base::Time now,
            bool new_session) {
    InitSession(session_start, last_active, now);
    CHECK(!idle_policy_);
    auto policy_ptr =
        std::make_unique<testing::StrictMock<test::MockIdlePolicy>>();
    idle_policy_ = policy_ptr.get();

    if (new_state.application_active) {
      EXPECT_CALL(idle_policy(), IsActive(new_state.last_active_time))
          .WillOnce(testing::Return(true));
      EXPECT_CALL(idle_policy(), IsNewSession(session_start, last_active,
                                              new_state.last_active_time))
          .WillOnce(testing::Return(new_session));
    }
    EXPECT_CALL(session_manager(), OnIdleStateUpdating(new_state));
    if (new_session) {
      EXPECT_CALL(session_manager(), OnNewSession(session_start, last_active,
                                                  new_state.last_active_time));
    }
    InitSessionManager(new_state, std::move(policy_ptr));
    CheckSessionData(new_session ? new_state.last_active_time : session_start,
                     new_state.application_active ? new_state.last_active_time
                                                  : last_active);
  }

 private:
  raw_ptr<test::MockIdlePolicy> idle_policy_ = nullptr;
};

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest,
       StartJustAfterLastActive_NoNewSession) {
  const auto now = base::Time::Now();
  Init(now - base::Hours(4), now - base::Minutes(2),
       IdleState{now - base::Minutes(1)}, now, /*new_session=*/false);
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest,
       StartWellAfterLastActive_NewSession) {
  const auto now = base::Time::Now();
  Init(now - base::Days(2), now - base::Days(1),
       IdleState{now - base::Minutes(1)}, now, /*new_session=*/true);
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest,
       StartApplicationInactive_NoNewSession) {
  const auto now = base::Time::Now();
  Init(now - base::Days(2), now - base::Days(1),
       IdleState{now - base::Minutes(1), false}, now, /*new_session=*/false);
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest,
       IsActiveFalseMeansNoNewSession) {
  const auto now = base::Time::Now();
  const auto session_start = now - base::Hours(4);
  Init(session_start, now - base::Minutes(2), IdleState{now - base::Minutes(1)},
       now, /*new_session=*/false);
  const auto new_active_time = now + base::Hours(1);
  const auto new_now = new_active_time + base::Minutes(1);

  EXPECT_CALL(idle_policy(), IsActive(new_active_time))
      .WillOnce(testing::Return(false));
  UpdateState(IdleState{new_active_time}, new_now, /*expect_new_session=*/false,
              /*suppress_last_active_update=*/true);
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest, SystemInactiveNoUpdate) {
  const auto now = base::Time::Now();
  const auto session_start = now - base::Hours(4);
  const auto last_active = now - base::Minutes(1);
  Init(session_start, now - base::Minutes(2), IdleState{last_active}, now,
       /*new_session=*/false);
  const auto new_active_time = now + base::Hours(1);
  const auto new_now = new_active_time + base::Seconds(10);

  UpdateState(IdleState{new_active_time, false}, new_now, false);
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest,
       IsActiveTrueButNoNewSession) {
  const auto now = base::Time::Now();
  const auto session_start = now - base::Hours(4);
  const auto last_active = now - base::Minutes(1);
  Init(session_start, now - base::Minutes(2), IdleState{last_active}, now,
       /*new_session=*/false);
  const auto new_active_time = now + base::Hours(1);
  const auto new_now = new_active_time + base::Seconds(10);

  EXPECT_CALL(idle_policy(), IsActive(new_active_time))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(idle_policy(),
              IsNewSession(session_start, last_active, new_active_time))
      .WillOnce(testing::Return(false));
  UpdateState(IdleState{new_active_time}, new_now, false);
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest,
       IsActiveTrueAndNewSession) {
  const auto now = base::Time::Now();
  const auto session_start = now - base::Hours(4);
  const auto last_active = now - base::Minutes(1);
  Init(session_start, now - base::Minutes(2), IdleState{last_active}, now,
       /*new_session=*/false);
  const auto new_active_time = now + base::Hours(1);
  const auto new_now = new_active_time + base::Seconds(10);

  EXPECT_CALL(idle_policy(), IsActive(new_active_time))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(idle_policy(),
              IsNewSession(session_start, last_active, new_active_time))
      .WillOnce(testing::Return(true));
  UpdateState(IdleState{new_active_time}, new_now, true);
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest, IsApplicationActiveTrue) {
  const auto now = base::Time::Now();
  const auto last_active = now - base::Minutes(1);
  Init(now - base::Hours(4), now - base::Minutes(2), IdleState{last_active},
       now, /*new_session=*/false);
  EXPECT_CALL(idle_policy(), IsActive(last_active))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(session_manager().IsApplicationActive());
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest, IsApplicationActiveFalse) {
  const auto now = base::Time::Now();
  const auto last_active = now - base::Minutes(1);
  Init(now - base::Hours(4), now - base::Minutes(2), IdleState{last_active},
       now, /*new_session=*/false);
  EXPECT_CALL(idle_policy(), IsActive(last_active))
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(session_manager().IsApplicationActive());
}

// Class that tests the functionality of the IdlePolicy in conjunction with the
// FeaturePromoSessionManager.
class FeaturePromoSessionManagerIdlePolicyTest
    : public FeaturePromoSessionManagerWithMockManagerTest {
 public:
  FeaturePromoSessionManagerIdlePolicyTest() = default;
  ~FeaturePromoSessionManagerIdlePolicyTest() override = default;

 protected:
  static constexpr base::TimeDelta kTimeToIdle = base::Seconds(30);
  static constexpr base::TimeDelta kIdleTimeBetweenSessions = base::Hours(3);
  static constexpr base::TimeDelta kMinimumSessionLength = base::Hours(4);

  // Performs initialization and returns whether a new session was generated.
  // If a new session is expected but not generated, then an expected call on
  // the mock session manager will be wrong, and the test will fail.
  bool Init(base::Time session_start,
            base::Time last_active,
            IdleState new_state,
            base::Time now) {
    InitSession(session_start, last_active, now);
    EXPECT_CALL(session_manager(), OnIdleStateUpdating(new_state));
    const bool is_idle = !new_state.application_active ||
                         (now - new_state.last_active_time) >= kTimeToIdle;
    const bool new_session =
        !is_idle &&
        (new_state.last_active_time - last_active) >=
            kIdleTimeBetweenSessions &&
        (new_state.last_active_time - session_start) >= kMinimumSessionLength;
    if (new_session) {
      EXPECT_CALL(session_manager(), OnNewSession(session_start, last_active,
                                                  new_state.last_active_time));
    }
    InitSessionManager(
        new_state,
        base::WrapUnique(new FeaturePromoSessionManager::IdlePolicy(
            kTimeToIdle, kIdleTimeBetweenSessions, kMinimumSessionLength)));
    CheckSessionData(new_session ? new_state.last_active_time : session_start,
                     is_idle ? last_active : new_state.last_active_time);
    return new_session;
  }

  // Performs initialization creating a new session and returns the most recent
  // start and active time.
  FeaturePromoSessionData InitWithStandardParams() {
    const auto start = base::Time::Now();
    const auto first_active = start + base::Minutes(1);
    const auto second_active = start + base::Minutes(2);
    const bool new_session =
        Init(start, first_active, IdleState{second_active}, second_active);
    CHECK(!new_session);
    return storage_service().ReadSessionData();
  }

  bool Update(IdleState new_state, base::Time new_now) {
    const auto old_data = storage_service().ReadSessionData();
    const bool is_idle = !new_state.application_active ||
                         (new_now - new_state.last_active_time) >= kTimeToIdle;
    const bool new_session =
        !is_idle &&
        (new_state.last_active_time - old_data.most_recent_active_time) >=
            kIdleTimeBetweenSessions &&
        (new_state.last_active_time - old_data.start_time) >=
            kMinimumSessionLength;
    UpdateState(new_state, new_now, new_session, is_idle);
    return new_session;
  }
};

TEST_F(FeaturePromoSessionManagerIdlePolicyTest, InitApplicationNotActive) {
  const auto start_time = base::Time::Now();
  const auto update_time = start_time + base::Hours(2);
  const bool new_session =
      Init(start_time, start_time + base::Hours(1),
           IdleState{update_time, /*application_active=*/false},
           update_time + base::Seconds(5));
  EXPECT_FALSE(new_session);
}

TEST_F(FeaturePromoSessionManagerIdlePolicyTest,
       InitApplicationActiveNoNewSession) {
  const auto start_time = base::Time::Now();
  const auto old_time = start_time + kIdleTimeBetweenSessions / 4;
  const auto update_time = start_time + kIdleTimeBetweenSessions / 2;
  const bool new_session = Init(start_time, old_time, IdleState{update_time},
                                update_time + kTimeToIdle / 2);
  EXPECT_FALSE(new_session);
  EXPECT_TRUE(session_manager().IsApplicationActive());
}

TEST_F(FeaturePromoSessionManagerIdlePolicyTest,
       InitApplicationIdleNoNewSession) {
  const auto start_time = base::Time::Now();
  const auto old_time = start_time + kIdleTimeBetweenSessions / 4;
  const auto update_time = start_time + kIdleTimeBetweenSessions / 2;
  const bool new_session = Init(start_time, old_time, IdleState{update_time},
                                update_time + kTimeToIdle + base::Seconds(2));
  EXPECT_FALSE(new_session);
  EXPECT_FALSE(session_manager().IsApplicationActive());
}

TEST_F(FeaturePromoSessionManagerIdlePolicyTest,
       InitApplicationActiveNewSession) {
  const auto start_time = base::Time::Now();
  const auto old_time = start_time + kIdleTimeBetweenSessions / 2;
  const auto update_time =
      old_time + kIdleTimeBetweenSessions + base::Minutes(5);
  const bool new_session = Init(start_time, old_time, IdleState{update_time},
                                update_time + kTimeToIdle / 2);
  EXPECT_TRUE(new_session);
  EXPECT_TRUE(session_manager().IsApplicationActive());
}

// It would be unusual for a profile or browser to load while the machine is
// idle, but if it does a new session should not be started right away.
TEST_F(FeaturePromoSessionManagerIdlePolicyTest,
       InitApplicationIdleAfterGapNoNewSession) {
  const auto start_time = base::Time::Now();
  const auto old_time = start_time + kIdleTimeBetweenSessions / 2;
  const auto update_time =
      old_time + kIdleTimeBetweenSessions + base::Minutes(5);
  const bool new_session = Init(start_time, old_time, IdleState{update_time},
                                update_time + kTimeToIdle + base::Seconds(2));
  EXPECT_FALSE(new_session);
  EXPECT_FALSE(session_manager().IsApplicationActive());
}

TEST_F(FeaturePromoSessionManagerIdlePolicyTest,
       StateUpdatedActiveNoNewSessionDueToMinimumSessionLength) {
  const auto state = InitWithStandardParams();
  const auto new_active =
      state.start_time + kMinimumSessionLength - base::Seconds(1);
  const bool new_session =
      Update(IdleState{new_active},
             new_active + kTimeToIdle - base::Milliseconds(500));
  EXPECT_FALSE(new_session);
}

TEST_F(FeaturePromoSessionManagerIdlePolicyTest, StateUpdatedActiveNewSession) {
  const auto state = InitWithStandardParams();
  const auto new_active =
      state.most_recent_active_time + kMinimumSessionLength + base::Seconds(1);
  const bool new_session =
      Update(IdleState{new_active},
             new_active + kTimeToIdle - base::Milliseconds(500));
  EXPECT_TRUE(new_session);
}

TEST_F(FeaturePromoSessionManagerIdlePolicyTest,
       StateUpdatedInactiveNoNewSession) {
  const auto state = InitWithStandardParams();
  const auto new_active =
      state.most_recent_active_time + kMinimumSessionLength + base::Seconds(1);
  const bool new_session =
      Update(IdleState{new_active, false},
             new_active + kTimeToIdle - base::Milliseconds(500));
  EXPECT_FALSE(new_session);
}

TEST_F(FeaturePromoSessionManagerIdlePolicyTest, StateUpdatedIdleNoNewSession) {
  const auto state = InitWithStandardParams();
  const auto new_active =
      state.most_recent_active_time + kMinimumSessionLength + base::Seconds(1);
  const bool new_session =
      Update(IdleState{new_active},
             new_active + kTimeToIdle + base::Milliseconds(500));
  EXPECT_FALSE(new_session);
}

TEST_F(FeaturePromoSessionManagerIdlePolicyTest,
       StateUpdatedInsideThenOutsideMinimumSession_NewSession) {
  const auto state = InitWithStandardParams();
  const auto checkpoint =
      state.start_time + kMinimumSessionLength - base::Minutes(5);
  const auto final = checkpoint + kIdleTimeBetweenSessions + base::Minutes(5);
  CHECK_GT(final - state.start_time, kMinimumSessionLength);
  // Push out to near minimum session length.
  Update(IdleState{checkpoint}, checkpoint + base::Milliseconds(500));
  // Wait until new session time has passed from previous update.
  const bool new_session =
      Update(IdleState{final}, final + base::Milliseconds(500));
  EXPECT_TRUE(new_session);
}

TEST_F(FeaturePromoSessionManagerIdlePolicyTest,
       StateUpdatedInsideThenOutsideMinimumSession_NoNewSession) {
  const auto state = InitWithStandardParams();
  const auto checkpoint =
      state.start_time + kMinimumSessionLength - base::Minutes(5);
  const auto final = checkpoint + kIdleTimeBetweenSessions - base::Minutes(5);
  CHECK_GT(final - state.start_time, kMinimumSessionLength);
  // Push out to near minimum session length.
  Update(IdleState{checkpoint}, checkpoint + base::Milliseconds(500));
  // Wait until slightly less than new session time has passed from previous
  // update.
  const bool new_session =
      Update(IdleState{final}, final + base::Milliseconds(500));
  EXPECT_FALSE(new_session);
}

// Regression test for a case where on some computers, returning from sleep and
// then leaving the locked state would first update the most recent active time,
// then fail to register the new session when the program became active, which
// meant some users would not experience any new sessions.
TEST_F(FeaturePromoSessionManagerIdlePolicyTest, ReturnFromLockedNewSession) {
  const auto state = InitWithStandardParams();
  const auto locked = state.start_time + kMinimumSessionLength * 1.5;
  const auto subsequent = locked + base::Seconds(15);
  // Push out to well beyond minimum session length/time between sessions and
  // simulate wake from sleep to a lock screen.
  Update(IdleState{locked, false}, locked + base::Seconds(1));
  // Simulate the unlock and the application becoming active.
  const bool new_session =
      Update(IdleState{subsequent}, subsequent + base::Seconds(1));
  EXPECT_TRUE(new_session);
}

TEST_F(FeaturePromoSessionManagerIdlePolicyTest, ReturnFromLockedNoNewSession) {
  const auto state = InitWithStandardParams();
  const auto update = state.most_recent_active_time + kMinimumSessionLength / 2;
  const auto locked = update + kIdleTimeBetweenSessions + base::Minutes(5);
  const auto subsequent = locked + base::Seconds(15);
  CHECK_GT(subsequent, state.start_time + kMinimumSessionLength);
  // Push out well into the minimum session time.
  Update(IdleState{update}, update + base::Seconds(1));
  // Wake from sleep to locked state, without crossing the minimum time between
  // sessions from the previous active state.
  Update(IdleState{locked, false}, locked + base::Seconds(1));
  // Simulate unlock and switch to active, still inside minimum time between
  // sessions.
  const bool new_session =
      Update(IdleState{subsequent}, subsequent + base::Seconds(1));
  EXPECT_TRUE(new_session);
}

// Test that `IsApplicationActive()` returns the correct values as the current
// time becomes further from the last updated active time.
TEST_F(FeaturePromoSessionManagerIdlePolicyTest, IsApplicationActive) {
  const auto state = InitWithStandardParams();

  // Move forward in small increments verifying when the reported activity
  // becomes idle.
  EXPECT_TRUE(session_manager().IsApplicationActive());
  clock().Advance(kTimeToIdle / 2);
  EXPECT_TRUE(session_manager().IsApplicationActive());
  clock().Advance(kTimeToIdle / 3);
  EXPECT_TRUE(session_manager().IsApplicationActive());
  clock().Advance(kTimeToIdle / 3);
  EXPECT_FALSE(session_manager().IsApplicationActive());

  // Update a new more recent active event and move forward again.
  const auto new_now = clock().Now();
  Update(IdleState{new_now}, new_now + kTimeToIdle / 3);
  EXPECT_TRUE(session_manager().IsApplicationActive());
  clock().Advance(kTimeToIdle);
  EXPECT_FALSE(session_manager().IsApplicationActive());
}

TEST_F(FeaturePromoSessionManagerIdlePolicyTest,
       IsApplicationActiveDuringAndAfterLock) {
  const auto state = InitWithStandardParams();
  const auto update_time = state.most_recent_active_time + base::Seconds(1);

  // Simulate a locked machine and advance time; the application should never
  // report as active.
  Update(IdleState{update_time, false}, update_time);
  EXPECT_FALSE(session_manager().IsApplicationActive());
  clock().Advance(kTimeToIdle / 2);
  EXPECT_FALSE(session_manager().IsApplicationActive());
  clock().Advance(kTimeToIdle);
  EXPECT_FALSE(session_manager().IsApplicationActive());

  // Now simulate unlock.
  const auto second_time = update_time + base::Minutes(2);
  Update(IdleState{second_time}, second_time);
  EXPECT_TRUE(session_manager().IsApplicationActive());
}

}  // namespace user_education
