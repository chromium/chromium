// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_hub_vector_lifecycle.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/impl/auth_parts_impl.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_engine.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

constexpr base::TimeDelta kLongTime = base::Minutes(1);
constexpr AshAuthFactor kFactor = AshAuthFactor::kGaiaPassword;
constexpr AshAuthFactor kSpecialFactor = AshAuthFactor::kCryptohomePin;

using base::test::RunOnceCallback;
using base::test::RunOnceClosure;
using testing::_;
using testing::AnyNumber;
using testing::AtMost;
using testing::ByMove;
using testing::DoAll;
using testing::Eq;
using testing::InSequence;
using testing::Invoke;
using testing::Mock;
using testing::MockFunction;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

class MockVectorLifecycleOwner : public AuthHubVectorLifecycle::Owner {
 public:
  MockVectorLifecycleOwner() = default;
  ~MockVectorLifecycleOwner() override = default;

  MOCK_METHOD(AuthFactorEngine::FactorEngineObserver*,
              AsEngineObserver,
              (),
              (override));
  MOCK_METHOD(void,
              OnAttemptStarted,
              (const AuthAttemptVector&, AuthFactorsSet, AuthFactorsSet),
              (override));
  MOCK_METHOD(void, OnAttemptFinished, (const AuthAttemptVector&), (override));
  MOCK_METHOD(void, OnAttemptCancelled, (const AuthAttemptVector&), (override));
  MOCK_METHOD(void, OnAttemptCleanedUp, (const AuthAttemptVector&), (override));
  MOCK_METHOD(void, OnIdle, (), (override));
};

class AuthHubVectorLifecycleTest : public ::testing::Test {
 protected:
  AuthHubVectorLifecycleTest() {
    parts_ = AuthPartsImpl::CreateTestInstance();
    account_ = AccountId::FromUserEmail("user1@example.com");
  }

  void SetUp() override {}

  void InitLifecycle() {
    AuthEnginesMap engines;
    for (const auto& engine : engines_) {
      engines[engine.first] = engine.second.get();
    }
    lifecycle_ = std::make_unique<AuthHubVectorLifecycle>(
        &owner_, AuthHubMode::kLoginScreen, engines);
    EXPECT_CALL(owner_, AsEngineObserver())
        .Times(AnyNumber())
        .WillRepeatedly(Return(&owner_engine_observer_));
  }

  void ExpectOwnerAttemptStartCalled(AuthAttemptVector attempt) {
    EXPECT_CALL(owner_, OnAttemptStarted(Eq(attempt), _, _))
        .WillOnce(Invoke([&](const AuthAttemptVector&, AuthFactorsSet usable,
                             AuthFactorsSet failed) {
          usable_factors_ = usable;
          failed_factors_ = failed;
        }));
  }

  void TriggerTimeout() { task_environment_.FastForwardBy(kLongTime); }

  void ExpectFactorAvailability(AuthFactorsSet usable, AuthFactorsSet failed) {
    ASSERT_EQ(usable_factors_, usable);
    ASSERT_EQ(failed_factors_, failed);
  }

  void StartAttemptWithAllFactors(AuthAttemptVector attempt) {
    AuthFactorsSet all_factors;
    lifecycle_->StartAttempt(attempt);
    ExpectOwnerAttemptStartCalled(attempt);
    for (const auto& observer : engine_obvservers_) {
      all_factors.Put(observer.first);
      observer.second->OnFactorPresenceChecked(observer.first, true);
    }
    ExpectFactorAvailability(all_factors, AuthFactorsSet{});
    for (const auto& observer : engine_obvservers_) {
      EXPECT_EQ(observer.second.get(), &owner_engine_observer_);
    }
  }

  void ExpectAllFactorsShutdown() {
    for (const auto& engine : engines_) {
      EXPECT_CALL(*engine.second, CleanUp(_))
          .WillOnce(RunOnceCallback<0>(engine.first));
      EXPECT_CALL(*engine.second, StopAuthFlow(_))
          .WillOnce(RunOnceCallback<0>(engine.first));
    }
  }

  void ExpectAuthOnEngine(AshAuthFactor factor, AuthAttemptVector attempt) {
    StrictMock<MockAuthFactorEngine>* engine = engines_[factor].get();

    EXPECT_CALL(*engine,
                StartAuthFlow(Eq(attempt.account), Eq(attempt.purpose), _))
        .WillOnce(Invoke(
            [&, factor](AccountId, AuthPurpose,
                        AuthFactorEngine::FactorEngineObserver* observer) {
              engine_obvservers_[factor] = observer;
            }));
  }

  StrictMock<MockAuthFactorEngine>* SetUpEngine(AshAuthFactor factor) {
    engines_[factor] = std::make_unique<StrictMock<MockAuthFactorEngine>>();
    StrictMock<MockAuthFactorEngine>* engine = engines_[factor].get();

    ExpectAuthOnEngine(factor,
                       AuthAttemptVector{account_, AuthPurpose::kLogin});

    EXPECT_CALL(*engine, UpdateObserver(_))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(
            [&, factor](AuthFactorEngine::FactorEngineObserver* observer) {
              engine_obvservers_[factor] = observer;
            }));
    return engine;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  AccountId account_;
  std::unique_ptr<AuthPartsImpl> parts_;
  StrictMock<MockVectorLifecycleOwner> owner_;
  StrictMock<MockAuthFactorEngineObserver> owner_engine_observer_;

  base::flat_map<AshAuthFactor,
                 std::unique_ptr<StrictMock<MockAuthFactorEngine>>>
      engines_;
  base::flat_map<
      AshAuthFactor,
      raw_ptr<AuthFactorEngine::FactorEngineObserver, DanglingUntriaged>>
      engine_obvservers_;
  AuthFactorsSet usable_factors_;
  AuthFactorsSet failed_factors_;

  std::unique_ptr<AuthHubVectorLifecycle> lifecycle_;

  raw_ptr<AuthHubConnector> connector_;
};

// Standard init/shutdown flow.
TEST_F(AuthHubVectorLifecycleTest, SingleFactorAttemptStartCancel) {
  auto* engine = SetUpEngine(kFactor);
  InitLifecycle();

  EXPECT_TRUE(lifecycle_->IsIdle());

  AuthAttemptVector attempt{account_, AuthPurpose::kLogin};

  // Start auth attempt, Lifecycle sets observer to engine.
  lifecycle_->StartAttempt(attempt);

  EXPECT_FALSE(lifecycle_->IsIdle());

  ExpectOwnerAttemptStartCalled(attempt);

  // Report that engine is ready.
  engine_obvservers_[kFactor]->OnFactorPresenceChecked(kFactor, true);

  // Upon initialization Lifecycle should report usable factors, and
  // update Observer to Owner for them.
  ExpectFactorAvailability(AuthFactorsSet{kFactor}, AuthFactorsSet{});

  EXPECT_EQ(engine_obvservers_[kFactor].get(), &owner_engine_observer_);
  EXPECT_FALSE(lifecycle_->IsIdle());

  Mock::VerifyAndClearExpectations(&owner_);

  AuthFactorEngine::CleanupCallback cleanup_callback;
  AuthFactorEngine::ShutdownCallback shutdown_callback;
  EXPECT_CALL(*engine, CleanUp(_)).WillOnce(MoveArg<0>(&cleanup_callback));
  EXPECT_CALL(*engine, StopAuthFlow(_))
      .WillOnce(MoveArg<0>(&shutdown_callback));
  MockFunction<void(std::string check_point_name)> check;
  {
    InSequence s;
    EXPECT_CALL(owner_, OnAttemptCleanedUp(Eq(attempt)));
    EXPECT_CALL(check, Call("NotifyCleanedUp"));
    EXPECT_CALL(owner_, OnAttemptCancelled(Eq(attempt)));
    EXPECT_CALL(owner_, OnIdle());
    EXPECT_CALL(check, Call("NotifyFinished"));
  }
  lifecycle_->CancelAttempt();

  // Upon attempt cancellation, Lifecycle should provide callback
  // to CleanUp first.
  ASSERT_FALSE(cleanup_callback.is_null());

  // Once engine finishes attempt cleanup, Lifecycle should notify the Owner,
  // and provide callback to StopAuthFlow.
  std::move(cleanup_callback).Run(kFactor);
  check.Call("NotifyCleanedUp");
  ASSERT_FALSE(shutdown_callback.is_null());

  // Once engine finishes shutdown, Lifecycle should
  // notify the Owner.
  std::move(shutdown_callback).Run(kFactor);
  check.Call("NotifyFinished");

  EXPECT_TRUE(lifecycle_->IsIdle());
}

TEST_F(AuthHubVectorLifecycleTest, FactorNotPresent) {
  SetUpEngine(kFactor);
  SetUpEngine(kSpecialFactor);
  InitLifecycle();

  EXPECT_TRUE(lifecycle_->IsIdle());

  AuthAttemptVector attempt{account_, AuthPurpose::kLogin};
  // Start auth attempt, Lifecycle sets observer to engine.
  lifecycle_->StartAttempt(attempt);
  EXPECT_FALSE(lifecycle_->IsIdle());

  ExpectOwnerAttemptStartCalled(attempt);

  engine_obvservers_[kFactor]->OnFactorPresenceChecked(kFactor, true);
  engine_obvservers_[kSpecialFactor]->OnFactorPresenceChecked(kSpecialFactor,
                                                              false);

  // Upon initialization Lifecycle should report usable factors, and
  // update Observer to Owner for them.
  // Note that as kSpecialFactor is just not configured, it is not reported
  // neither in `failed_factors` nor in `usable_factors`, it's observer is not
  // changed.

  ExpectFactorAvailability(AuthFactorsSet{kFactor}, AuthFactorsSet{});

  EXPECT_EQ(engine_obvservers_[kFactor].get(), &owner_engine_observer_);
  EXPECT_NE(engine_obvservers_[kSpecialFactor].get(), &owner_engine_observer_);
  EXPECT_FALSE(lifecycle_->IsIdle());

  // Upon cleanup, non-present factor should still be asked
  // to stop auth flow.
  ExpectAllFactorsShutdown();

  EXPECT_CALL(owner_, OnAttemptCleanedUp(_));
  EXPECT_CALL(owner_, OnAttemptCancelled(_));
  EXPECT_CALL(owner_, OnIdle());

  lifecycle_->CancelAttempt();
}

TEST_F(AuthHubVectorLifecycleTest, FactorTimedOutDuringInit) {
  SetUpEngine(kFactor);
  auto* timeout_engine = SetUpEngine(kSpecialFactor);
  InitLifecycle();

  EXPECT_TRUE(lifecycle_->IsIdle());

  AuthAttemptVector attempt{account_, AuthPurpose::kLogin};
  // Start auth attempt, Lifecycle sets observer to engine.
  lifecycle_->StartAttempt(attempt);
  EXPECT_FALSE(lifecycle_->IsIdle());

  ExpectOwnerAttemptStartCalled(attempt);

  engine_obvservers_[kFactor]->OnFactorPresenceChecked(kFactor, true);
  EXPECT_CALL(*timeout_engine, StartFlowTimedOut());

  TriggerTimeout();

  // Upon initialization Lifecycle should report usable and failed factors, and
  // update Observer to Owner for usable ones.
  ExpectFactorAvailability(AuthFactorsSet{kFactor},
                           AuthFactorsSet{kSpecialFactor});

  EXPECT_EQ(engine_obvservers_[kFactor].get(), &owner_engine_observer_);
  EXPECT_NE(engine_obvservers_[kSpecialFactor].get(), &owner_engine_observer_);
  EXPECT_FALSE(lifecycle_->IsIdle());

  // Upon cleanup, timed-out factor should still be asked
  // to stop auth flow.
  ExpectAllFactorsShutdown();

  EXPECT_CALL(owner_, OnAttemptCleanedUp(_));
  EXPECT_CALL(owner_, OnAttemptCancelled(_));
  EXPECT_CALL(owner_, OnIdle());

  lifecycle_->CancelAttempt();
}

TEST_F(AuthHubVectorLifecycleTest, FactorCriticalErrorDuringInit) {
  SetUpEngine(kFactor);
  SetUpEngine(kSpecialFactor);
  InitLifecycle();

  EXPECT_TRUE(lifecycle_->IsIdle());

  AuthAttemptVector attempt{account_, AuthPurpose::kLogin};
  // Start auth attempt, Lifecycle sets observer to engine.
  lifecycle_->StartAttempt(attempt);
  EXPECT_FALSE(lifecycle_->IsIdle());

  ExpectOwnerAttemptStartCalled(attempt);

  engine_obvservers_[kFactor]->OnFactorPresenceChecked(kFactor, true);
  engine_obvservers_[kFactor]->OnCriticalError(kSpecialFactor);

  // Upon initialization Lifecycle should report usable and failed factors, and
  // update Observer to Owner for usable ones.

  ExpectFactorAvailability(AuthFactorsSet{kFactor},
                           AuthFactorsSet{kSpecialFactor});

  EXPECT_EQ(engine_obvservers_[kFactor].get(), &owner_engine_observer_);
  EXPECT_NE(engine_obvservers_[kSpecialFactor].get(), &owner_engine_observer_);
  EXPECT_FALSE(lifecycle_->IsIdle());

  // Upon cleanup, failed factor should still be asked to stop auth flow.
  ExpectAllFactorsShutdown();

  EXPECT_CALL(owner_, OnAttemptCleanedUp(_));
  EXPECT_CALL(owner_, OnAttemptCancelled(_));
  EXPECT_CALL(owner_, OnIdle());

  lifecycle_->CancelAttempt();
}

TEST_F(AuthHubVectorLifecycleTest, FactorTimedOutDuringCleanup) {
  auto* engine = SetUpEngine(kFactor);
  auto* timeout_engine = SetUpEngine(kSpecialFactor);
  InitLifecycle();

  AuthAttemptVector attempt{account_, AuthPurpose::kLogin};
  StartAttemptWithAllFactors(attempt);

  EXPECT_CALL(*engine, CleanUp(_)).WillOnce(RunOnceCallback<0>(kFactor));
  EXPECT_CALL(*engine, StopAuthFlow(_)).WillOnce(RunOnceCallback<0>(kFactor));
  AuthFactorEngine::CleanupCallback callback;
  EXPECT_CALL(*timeout_engine, CleanUp(_)).WillOnce(MoveArg<0>(&callback));
  EXPECT_CALL(*timeout_engine, StopAuthFlow(_))
      .WillOnce(RunOnceCallback<0>(kSpecialFactor));
  lifecycle_->CancelAttempt();

  ASSERT_FALSE(callback.is_null());

  EXPECT_CALL(owner_, OnAttemptCleanedUp(_));
  EXPECT_CALL(owner_, OnAttemptCancelled(_));
  EXPECT_CALL(owner_, OnIdle());
  EXPECT_CALL(*timeout_engine, StopFlowTimedOut());

  TriggerTimeout();
}

TEST_F(AuthHubVectorLifecycleTest, FactorTimedOutDuringShutdown) {
  auto* engine = SetUpEngine(kFactor);
  auto* timeout_engine = SetUpEngine(kSpecialFactor);
  InitLifecycle();

  AuthAttemptVector attempt{account_, AuthPurpose::kLogin};
  StartAttemptWithAllFactors(attempt);

  EXPECT_CALL(*engine, CleanUp(_)).WillOnce(RunOnceCallback<0>(kFactor));
  EXPECT_CALL(*engine, StopAuthFlow(_)).WillOnce(RunOnceCallback<0>(kFactor));
  AuthFactorEngine::ShutdownCallback callback;
  EXPECT_CALL(*timeout_engine, CleanUp(_))
      .WillOnce(RunOnceCallback<0>(kSpecialFactor));
  EXPECT_CALL(*timeout_engine, StopAuthFlow(_)).WillOnce(MoveArg<0>(&callback));
  EXPECT_CALL(owner_, OnAttemptCleanedUp(_));
  lifecycle_->CancelAttempt();

  ASSERT_FALSE(callback.is_null());

  EXPECT_CALL(owner_, OnAttemptCancelled(_));
  EXPECT_CALL(owner_, OnIdle());
  EXPECT_CALL(*timeout_engine, StopFlowTimedOut());

  TriggerTimeout();
}

TEST_F(AuthHubVectorLifecycleTest, SwitchAttemptDuringInit) {
  auto* engine = SetUpEngine(kFactor);
  InitLifecycle();

  AuthAttemptVector attempt1{account_, AuthPurpose::kLogin};
  AuthAttemptVector attempt2{AccountId::FromUserEmail("user2@example.com"),
                             AuthPurpose::kLogin};

  lifecycle_->StartAttempt(attempt1);

  EXPECT_CALL(*engine, CleanUp(_)).WillOnce(RunOnceCallback<0>(kFactor));
  EXPECT_CALL(*engine, StopAuthFlow(_)).WillOnce(RunOnceCallback<0>(kFactor));
  ExpectAuthOnEngine(kFactor, attempt2);

  lifecycle_->StartAttempt(attempt2);
  // Now 1st attempt should be finished and 2nd attempt should take over.
  engine_obvservers_[kFactor]->OnFactorPresenceChecked(kFactor, true);

  ExpectOwnerAttemptStartCalled(attempt2);
  // Now the second attempt should be going.
  engine_obvservers_[kFactor]->OnFactorPresenceChecked(kFactor, true);
}

}  // namespace ash
