// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_session_manager.h"

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_idle_observer.h"
#include "components/user_education/common/feature_promo_idle_policy.h"
#include "components/user_education/test/feature_promo_session_mocks.h"
#include "components/user_education/test/test_feature_promo_storage_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"

namespace user_education {

// Base class that uses a test idle observer to test basic functionality of a
// FeaturePromoSessionManager.
class FeaturePromoSessionManagerTest : public testing::Test {
 public:
  FeaturePromoSessionManagerTest() = default;
  ~FeaturePromoSessionManagerTest() override = default;

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
      std::make_unique<test::TestIdleObserver>(base::Time::Now());
  auto policy_ptr = std::make_unique<FeaturePromoIdlePolicy>();
  FeaturePromoSessionManager manager;
  manager.Init(&storage_service(), std::move(observer_ptr),
               std::move(policy_ptr));
  // Last active time was over half an hour ago.
  EXPECT_EQ(start_time, storage_service().ReadSessionData().start_time);
}

TEST_F(FeaturePromoSessionManagerTest, CheckIdlePolicyDefaults) {
  // Start in the middle of a session, currently active.
  const auto now = base::Time::Now();
  const auto start_time = now - base::Hours(4);
  InitSession(start_time, now, now);

  auto observer_ptr = std::make_unique<test::TestIdleObserver>(now);
  auto policy_ptr = std::make_unique<FeaturePromoIdlePolicy>();

  auto* const observer = observer_ptr.get();

  FeaturePromoSessionManager manager;
  manager.Init(&storage_service(), std::move(observer_ptr),
               std::move(policy_ptr));

  // Moving just a little bit later should not result in a new session.
  const auto kALittleLater = now + base::Milliseconds(500);
  const auto kALittleLaterNow = kALittleLater + base::Milliseconds(500);
  clock().SetNow(kALittleLaterNow);
  observer->SetLastActiveTime(kALittleLater, /*send_update=*/true);
  CheckSessionData(start_time, kALittleLater);

  // Moving to a much later time will result in a new session if everything is
  // configured properly.
  const auto kMuchLater = now + base::Days(5);
  const auto kMuchLaterNow = kMuchLater + base::Seconds(1);
  clock().SetNow(kMuchLaterNow);
  observer->SetLastActiveTime(kMuchLater, /*send_update=*/true);
  CheckSessionData(kMuchLater, kMuchLater);
}

TEST_F(FeaturePromoSessionManagerTest, CheckCallbackNoInitialSession) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, new_session_callback);
  const auto now = base::Time::Now();
  const auto start_time = now - base::Hours(4);
  InitSession(start_time, now, now);

  auto observer_ptr = std::make_unique<test::TestIdleObserver>(now);
  auto policy_ptr = std::make_unique<FeaturePromoIdlePolicy>();
  FeaturePromoSessionManager manager;
  manager.Init(&storage_service(), std::move(observer_ptr),
               std::move(policy_ptr));

  EXPECT_FALSE(manager.new_session_since_startup());
  auto subscription = manager.AddNewSessionCallback(new_session_callback.Get());
}

TEST_F(FeaturePromoSessionManagerTest, CheckCallbackWithInitialSession) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, new_session_callback);
  const auto now = base::Time::Now();
  const auto start_time = now - base::Hours(4);
  InitSession(start_time, now, now);

  auto observer_ptr = std::make_unique<test::TestIdleObserver>(now);
  auto policy_ptr = std::make_unique<FeaturePromoIdlePolicy>();
  auto* const observer = observer_ptr.get();
  FeaturePromoSessionManager manager;
  manager.Init(&storage_service(), std::move(observer_ptr),
               std::move(policy_ptr));

  // Moving to a much later time will result in a new session if everything is
  // configured properly.
  const auto kMuchLater = now + base::Days(5);
  const auto kMuchLaterNow = kMuchLater + base::Seconds(1);
  clock().SetNow(kMuchLaterNow);
  observer->SetLastActiveTime(kMuchLater, /*send_update=*/true);

  EXPECT_TRUE(manager.new_session_since_startup());
  auto subscription = manager.AddNewSessionCallback(new_session_callback.Get());
}

TEST_F(FeaturePromoSessionManagerTest, CheckCallbackCalledOnNewSession) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, new_session_callback);
  const auto now = base::Time::Now();
  const auto start_time = now - base::Hours(4);
  InitSession(start_time, now, now);

  auto observer_ptr = std::make_unique<test::TestIdleObserver>(now);
  auto policy_ptr = std::make_unique<FeaturePromoIdlePolicy>();
  auto* const observer = observer_ptr.get();
  FeaturePromoSessionManager manager;
  manager.Init(&storage_service(), std::move(observer_ptr),
               std::move(policy_ptr));

  auto subscription = manager.AddNewSessionCallback(new_session_callback.Get());

  // Moving just a little bit later should not result in a new session.
  const auto kALittleLater = now + base::Milliseconds(500);
  const auto kALittleLaterNow = kALittleLater + base::Milliseconds(500);
  clock().SetNow(kALittleLaterNow);
  observer->SetLastActiveTime(kALittleLater, /*send_update=*/true);

  // Moving to a much later time will result in a new session if everything is
  // configured properly.
  const auto kMuchLater = now + base::Days(5);
  const auto kMuchLaterNow = kMuchLater + base::Seconds(1);
  clock().SetNow(kMuchLaterNow);
  EXPECT_CALL_IN_SCOPE(
      new_session_callback, Run,
      observer->SetLastActiveTime(kMuchLater, /*send_update=*/true));
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

  test::TestIdleObserver& idle_observer() { return *idle_observer_; }

  void InitSessionManager(std::unique_ptr<FeaturePromoIdlePolicy> idle_policy) {
    EXPECT_CALL(session_manager(), OnLastActiveTimeUpdating);
    session_manager_.Init(&storage_service(), CreateIdleObserver(),
                          std::move(idle_policy));
  }

  // Moves the clock forward and updates the current idle state and verifies the
  // expected update calls.
  //
  // If `suppress_last_active_update` is true, does not expect the last active
  // time to be updated.
  void UpdateState(std::optional<base::Time> new_last_active,
                   base::Time new_now,
                   bool expect_new_session,
                   bool suppress_last_active_update = false) {
    const bool send_update = new_last_active && !suppress_last_active_update;
    if (send_update) {
      EXPECT_CALL(session_manager(),
                  OnLastActiveTimeUpdating(*new_last_active));
    }
    const auto data = storage_service().ReadSessionData();
    if (expect_new_session) {
      CHECK(new_last_active);
      CHECK(!suppress_last_active_update);
      EXPECT_CALL(session_manager(),
                  OnNewSession(data.start_time, data.most_recent_active_time,
                               *new_last_active));
    }
    clock().SetNow(new_now);
    idle_observer_->SetLastActiveTime(new_last_active, send_update);
    CheckSessionData(expect_new_session ? *new_last_active : data.start_time,
                     new_last_active && !suppress_last_active_update
                         ? *new_last_active
                         : data.most_recent_active_time);
  }

 private:
  std::unique_ptr<test::TestIdleObserver> CreateIdleObserver() {
    CHECK(!idle_observer_);
    auto ptr = std::make_unique<test::TestIdleObserver>(clock().Now());
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
            base::Time previous_last_active,
            base::Time now,
            bool new_session) {
    InitSession(session_start, previous_last_active, now);
    CHECK(!idle_policy_);
    auto policy_ptr =
        std::make_unique<testing::StrictMock<test::MockIdlePolicy>>();
    idle_policy_ = policy_ptr.get();

    EXPECT_CALL(idle_policy(),
                IsNewSession(session_start, previous_last_active, now))
        .WillOnce(testing::Return(new_session));
    if (new_session) {
      EXPECT_CALL(session_manager(),
                  OnNewSession(session_start, previous_last_active, now));
    }
    InitSessionManager(std::move(policy_ptr));
    CheckSessionData(new_session ? now : session_start, now);
  }

 private:
  raw_ptr<test::MockIdlePolicy> idle_policy_ = nullptr;
};

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest,
       StartJustAfterLastActive_NoNewSession) {
  const auto now = base::Time::Now();
  Init(now - base::Hours(4), now - base::Minutes(2), now,
       /*new_session=*/false);
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest,
       StartWellAfterLastActive_NewSession) {
  const auto now = base::Time::Now();
  Init(now - base::Days(2), now - base::Days(1), now,
       /*new_session=*/true);
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest,
       StartApplicationInactive_NoNewSession) {
  const auto now = base::Time::Now();
  Init(now - base::Days(2), now - base::Days(1), now,
       /*new_session=*/false);
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest, SystemInactiveNoUpdate) {
  const auto now = base::Time::Now();
  const auto session_start = now - base::Hours(4);
  Init(session_start, now - base::Minutes(2), now, /*new_session=*/false);
  const auto new_active_time = now + base::Hours(1);
  const auto new_now = new_active_time + base::Seconds(10);

  UpdateState(std::nullopt, new_now, false);
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest, NoNewSession) {
  const auto now = base::Time::Now();
  const auto session_start = now - base::Hours(4);
  Init(session_start, now - base::Minutes(2), now, /*new_session=*/false);
  const auto new_active_time = now + base::Hours(1);
  const auto new_now = new_active_time + base::Seconds(10);
  EXPECT_CALL(idle_policy(), IsNewSession(session_start, now, new_active_time))
      .WillOnce(testing::Return(false));
  UpdateState(new_active_time, new_now, false);
}

TEST_F(FeaturePromoSessionManagerWithMockPolicyTest, NewSession) {
  const auto now = base::Time::Now();
  const auto session_start = now - base::Hours(4);
  Init(session_start, now - base::Minutes(2), now, /*new_session=*/false);
  const auto new_active_time = now + base::Hours(1);
  const auto new_now = new_active_time + base::Seconds(10);

  EXPECT_CALL(idle_policy(), IsNewSession(session_start, now, new_active_time))
      .WillOnce(testing::Return(true));
  UpdateState(new_active_time, new_now, true);
}

// Class that tests the functionality of the IdlePolicy in conjunction with the
// FeaturePromoSessionManager.
class FeaturePromoIdlePolicyTest
    : public FeaturePromoSessionManagerWithMockManagerTest {
 public:
  FeaturePromoIdlePolicyTest() = default;
  ~FeaturePromoIdlePolicyTest() override = default;

 protected:
  static constexpr base::TimeDelta kIdleTimeBetweenSessions = base::Hours(3);
  static constexpr base::TimeDelta kMinimumSessionLength = base::Hours(4);

  // Performs initialization and returns whether a new session was generated.
  // If a new session is expected but not generated, then an expected call on
  // the mock session manager will be wrong, and the test will fail.
  bool Init(base::Time session_start,
            base::Time previous_last_active,
            base::Time now) {
    InitSession(session_start, previous_last_active, now);
    const bool new_session =
        (now - previous_last_active) >= kIdleTimeBetweenSessions &&
        (now - session_start) >= kMinimumSessionLength;
    if (new_session) {
      EXPECT_CALL(session_manager(),
                  OnNewSession(session_start, previous_last_active, now));
    }
    InitSessionManager(base::WrapUnique(new FeaturePromoIdlePolicy(
        kIdleTimeBetweenSessions, kMinimumSessionLength)));
    CheckSessionData(new_session ? now : session_start, now);
    return new_session;
  }

  // Performs initialization creating a new session and returns the most recent
  // start and active time.
  FeaturePromoSessionData InitWithStandardParams() {
    const auto start = base::Time::Now();
    const auto first_active = start + base::Minutes(1);
    const auto second_active = start + base::Minutes(2);
    const bool new_session = Init(start, first_active, second_active);
    CHECK(!new_session);
    return storage_service().ReadSessionData();
  }

  bool Update(std::optional<base::Time> new_last_active, base::Time new_now) {
    const auto old_data = storage_service().ReadSessionData();
    const bool new_session =
        new_last_active &&
        (*new_last_active - old_data.most_recent_active_time) >=
            kIdleTimeBetweenSessions &&
        (*new_last_active - old_data.start_time) >= kMinimumSessionLength;
    UpdateState(new_last_active, new_now, new_session);
    return new_session;
  }
};

TEST_F(FeaturePromoIdlePolicyTest, InitApplicationNotActive) {
  const auto start_time = base::Time::Now();
  const auto new_now = start_time + base::Hours(2);
  EXPECT_FALSE(Init(start_time, start_time + base::Hours(1), new_now));
}

TEST_F(FeaturePromoIdlePolicyTest, InitApplicationActiveNoNewSession) {
  const auto start_time = base::Time::Now();
  const auto old_time = start_time + kIdleTimeBetweenSessions / 4;
  const auto update_time = start_time + kIdleTimeBetweenSessions / 2;
  EXPECT_FALSE(Init(start_time, old_time, update_time));
}

TEST_F(FeaturePromoIdlePolicyTest, InitApplicationActiveNewSession) {
  const auto start_time = base::Time::Now();
  const auto old_time = start_time + kIdleTimeBetweenSessions / 2;
  const auto update_time =
      old_time + kIdleTimeBetweenSessions + base::Minutes(5);
  EXPECT_TRUE(Init(start_time, old_time, update_time));
}

TEST_F(FeaturePromoIdlePolicyTest,
       StateUpdatedActiveNoNewSessionDueToMinimumSessionLength) {
  const auto state = InitWithStandardParams();
  const auto new_active =
      state.start_time + kMinimumSessionLength - base::Seconds(1);
  EXPECT_FALSE(Update(new_active, new_active + base::Milliseconds(500)));
}

TEST_F(FeaturePromoIdlePolicyTest, StateUpdatedActiveNewSession) {
  const auto state = InitWithStandardParams();
  const auto new_active =
      state.most_recent_active_time + kMinimumSessionLength + base::Seconds(1);
  EXPECT_TRUE(Update(new_active, new_active + base::Milliseconds(500)));
}

TEST_F(FeaturePromoIdlePolicyTest, StateUpdatedInactiveNoNewSession) {
  const auto state = InitWithStandardParams();
  const auto new_now =
      state.most_recent_active_time + kMinimumSessionLength + base::Seconds(1);
  EXPECT_FALSE(Update(std::nullopt, new_now));
}

TEST_F(FeaturePromoIdlePolicyTest,
       StateUpdatedInsideThenOutsideMinimumSession_NewSession) {
  const auto state = InitWithStandardParams();
  const auto checkpoint =
      state.start_time + kMinimumSessionLength - base::Minutes(5);
  const auto final = checkpoint + kIdleTimeBetweenSessions + base::Minutes(5);
  CHECK_GT(final - state.start_time, kMinimumSessionLength);
  // Push out to near minimum session length.
  EXPECT_FALSE(Update(checkpoint, checkpoint + base::Milliseconds(500)));
  // Wait until new session time has passed from previous update.
  EXPECT_TRUE(Update(final, final + base::Milliseconds(500)));
}

TEST_F(FeaturePromoIdlePolicyTest,
       StateUpdatedInsideThenOutsideMinimumSession_NoNewSession) {
  const auto state = InitWithStandardParams();
  const auto checkpoint =
      state.start_time + kMinimumSessionLength - base::Minutes(5);
  const auto final = checkpoint + kIdleTimeBetweenSessions - base::Minutes(5);
  CHECK_GT(final - state.start_time, kMinimumSessionLength);
  // Push out to near minimum session length.
  EXPECT_FALSE(Update(checkpoint, checkpoint + base::Milliseconds(500)));
  // Wait until slightly less than new session time has passed from previous
  // update.
  EXPECT_FALSE(Update(final, final + base::Milliseconds(500)));
}

// Regression test for a case where on some computers, returning from sleep and
// then leaving the locked state would first update the most recent active time,
// then fail to register the new session when the program became active, which
// meant some users would not experience any new sessions.
TEST_F(FeaturePromoIdlePolicyTest, ReturnFromLockedNewSession) {
  const auto state = InitWithStandardParams();
  const auto locked = state.start_time + kMinimumSessionLength * 1.5;
  const auto subsequent = locked + base::Seconds(15);
  // Push out to well beyond minimum session length/time between sessions and
  // simulate wake from sleep to a lock screen.
  EXPECT_FALSE(Update(std::nullopt, locked));
  // Simulate the unlock and the application becoming active.
  EXPECT_TRUE(Update(subsequent, subsequent + base::Seconds(1)));
}

TEST_F(FeaturePromoIdlePolicyTest, ReturnFromLockedNoNewSession) {
  const auto state = InitWithStandardParams();
  const auto update = state.most_recent_active_time + kMinimumSessionLength / 2;
  const auto locked = update + kIdleTimeBetweenSessions - base::Minutes(5);
  const auto subsequent = locked + base::Seconds(15);
  CHECK_GT(subsequent, state.start_time + kMinimumSessionLength);
  // Push out well into the minimum session time.
  EXPECT_FALSE(Update(update, update + base::Seconds(1)));
  // Wake from sleep to locked state, without crossing the minimum time between
  // sessions from the previous active state.
  EXPECT_FALSE(Update(std::nullopt, locked));
  // Simulate unlock and switch to active, still inside minimum time between
  // sessions.
  EXPECT_FALSE(Update(subsequent, subsequent + base::Seconds(1)));
}

TEST_F(FeaturePromoIdlePolicyTest, MaybeUpdateSessionStateNoNewSession) {
  const base::Time session_start = base::Time::Now();
  const base::Time last_active = session_start + base::Minutes(30);
  const base::Time browser_start = last_active + kIdleTimeBetweenSessions / 2;
  Init(session_start, last_active, browser_start);
  // Advance less than a new session, but a significant time, and update the
  // last active time in the observer, but do not propagate to the session
  // manager (which would trigger an update).
  const base::Time now = browser_start + base::Seconds(15);
  clock().SetNow(now);
  idle_observer().SetLastActiveTime(now, false);
  CheckSessionData(session_start, browser_start);
  // This will check to see if a new session would be warranted; in this case it
  // is not, so no update happens.
  session_manager().MaybeUpdateSessionState();
  CheckSessionData(session_start, browser_start);
}

TEST_F(FeaturePromoIdlePolicyTest, MaybeUpdateSessionStateNewSession) {
  const base::Time session_start = base::Time::Now();
  const base::Time last_active = session_start + base::Minutes(30);
  const base::Time browser_start = last_active + base::Minutes(30);
  Init(session_start, last_active, browser_start);
  // Advance more than a new session and update the last active time in the
  // observer, but do not propagate to the session manager (which would trigger
  // an immediate update).
  const base::Time now =
      browser_start + kIdleTimeBetweenSessions + base::Seconds(15);
  clock().SetNow(now);
  idle_observer().SetLastActiveTime(now, false);
  CheckSessionData(session_start, browser_start);
  // Because more than the session length has passed, calling
  // `MaybeUpdateSessionState()` will trigger another check, and cause an update
  // and a new session.
  EXPECT_CALL(session_manager(), OnLastActiveTimeUpdating(now));
  EXPECT_CALL(session_manager(),
              OnNewSession(session_start, browser_start, now));
  session_manager().MaybeUpdateSessionState();
  CheckSessionData(now, now);
}

}  // namespace user_education
