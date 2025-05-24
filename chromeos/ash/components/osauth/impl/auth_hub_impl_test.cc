// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_hub_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/impl/auth_parts_impl.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_attempt_consumer.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_engine.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_status_consumer.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

constexpr base::TimeDelta kLongTime = base::Minutes(1);
constexpr AshAuthFactor kFactor = AshAuthFactor::kGaiaPassword;

using base::test::RunOnceCallback;
using testing::_;
using testing::AnyNumber;
using testing::ByMove;
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

class AuthHubTestBase : public ::testing::Test {
 protected:
  AuthHubTestBase() {
    user_manager::UserManagerImpl::RegisterPrefs(local_state_.registry());
    user_manager_ =
        std::make_unique<user_manager::FakeUserManager>(&local_state_);
    user_manager_->Initialize();
    factors_cache_ = std::make_unique<AuthFactorPresenceCache>(&local_state_);
    parts_ = AuthPartsImpl::CreateTestInstance();
    parts_->SetAuthHub(std::make_unique<AuthHubImpl>(factors_cache_.get()));

    auto factory = std::make_unique<StrictMock<MockAuthFactorEngineFactory>>();
    engine_factory_ = factory.get();
    parts_->RegisterEngineFactory(std::move(factory));

    SetupEngineForNextInit();
  }

  ~AuthHubTestBase() override { user_manager_->Destroy(); }

  void SetupEngineForNextInit() {
    testing::Mock::VerifyAndClearExpectations(engine_factory_.get());

    auto engine = std::make_unique<StrictMock<MockAuthFactorEngine>>();
    engine_ = engine.get();
    EXPECT_CALL(*engine, GetFactor()).WillRepeatedly(Return(kFactor));
    EXPECT_CALL(*engine, InitializeCommon(_))
        .Times(AnyNumber())
        .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(kFactor));
    EXPECT_CALL(*engine, ShutdownCommon(_))
        .Times(AnyNumber())
        .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(kFactor));

    EXPECT_CALL(*engine_factory_, GetFactor()).WillRepeatedly(Return(kFactor));
    EXPECT_CALL(*engine_factory_, CreateEngine(_))
        .WillOnce(Return(ByMove(std::move(engine))));
  }

  void ExpectEngineStart(AuthAttemptVector vector) {
    EXPECT_CALL(*engine_,
                StartAuthFlow(Eq(vector.account), Eq(vector.purpose), _))
        .WillOnce(SaveArg<2>(&engine_observer_));
    EXPECT_CALL(*engine_, UpdateObserver(_))
        .Times(AnyNumber())
        .WillRepeatedly(SaveArg<0>(&engine_observer_));
  }

  void ExpectEngineStop() {
    EXPECT_CALL(*engine_, CleanUp(_))
        .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(kFactor));
    EXPECT_CALL(*engine_, StopAuthFlow(_))
        .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<0>(kFactor));
  }

  void ConfigureFactorAsAvailable() {
    EXPECT_CALL(*engine_, IsDisabledByPolicy())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*engine_, IsLockedOut())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*engine_, IsFactorSpecificRestricted())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    EXPECT_CALL(*engine_, SetUsageAllowed(_))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke([&](AuthFactorEngine::UsageAllowed usage) {
          engine_usage_ = usage;
        }));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple local_state_;
  std::unique_ptr<AuthFactorPresenceCache> factors_cache_;
  ash::ScopedStubInstallAttributes install_attributes{
      ash::StubInstallAttributes::CreateConsumerOwned()};
  std::unique_ptr<user_manager::FakeUserManager> user_manager_;
  std::unique_ptr<AuthPartsImpl> parts_;

  raw_ptr<MockAuthFactorEngineFactory> engine_factory_ = nullptr;
  raw_ptr<MockAuthFactorEngine> engine_ = nullptr;
  raw_ptr<AuthFactorEngine::FactorEngineObserver, AcrossTasksDanglingUntriaged>
      engine_observer_ = nullptr;
  std::optional<AuthFactorEngine::UsageAllowed> engine_usage_;
};

using AuthHubTestMode = AuthHubTestBase;

TEST_F(AuthHubTestMode, CheckEnsureInitialized) {
  base::test::TestFuture<void> init_future;

  AuthHub::Get()->EnsureInitialized(init_future.GetCallback());

  AuthFactorEngine::CommonInitCallback init_callback;
  EXPECT_CALL(*engine_, InitializeCommon(_))
      .WillOnce(MoveArg<0>(&init_callback));

  AuthHub::Get()->InitializeForMode(AuthHubMode::kLoginScreen);

  EXPECT_FALSE(init_future.IsReady());

  std::move(init_callback).Run(kFactor);

  EXPECT_TRUE(init_future.IsReady());
}

class AuthHubTestVector : public AuthHubTestBase {
 protected:
  AuthHubTestVector() {
    account_ = AccountId::FromUserEmail("user1@example.com");
    attempt_ = AuthAttemptVector{account_, AuthPurpose::kLogin};
  }

  void ExpectAttemptConfirmation() {
    EXPECT_CALL(status_consumer_, InitializeUi(_, _))
        .WillOnce(
            Invoke([&](AuthFactorsSet factors, AuthHubConnector* connector) {
              for (auto factor : factors) {
                factors_state_[factor] = AuthFactorState::kCheckingForPresence;
              }
            }));

    EXPECT_CALL(attempt_consumer_, OnUserAuthAttemptConfirmed(_, _))
        .WillOnce(Invoke([&](AuthHubConnector* connector,
                             raw_ptr<AuthFactorStatusConsumer>& out_consumer) {
          connector_ = connector;
          out_consumer = &status_consumer_;
        }));
  }

  void ExpectFactorListUpdate() {
    EXPECT_CALL(status_consumer_, OnFactorListChanged(_))
        .WillOnce(
            Invoke([&](FactorsStatusMap update) { factors_state_ = update; }));
  }

  void ExpectFactorStateUpdate() {
    EXPECT_CALL(status_consumer_, OnFactorStatusesChanged(_))
        .WillOnce(Invoke([&](FactorsStatusMap update) {
          for (const auto& factor : update) {
            factors_state_[factor.first] = factor.second;
          }
        }));
  }

  AccountId account_;
  AuthAttemptVector attempt_;
  raw_ptr<AuthHubConnector, AcrossTasksDanglingUntriaged> connector_ = nullptr;
  StrictMock<MockAuthAttemptConsumer> attempt_consumer_;
  StrictMock<MockAuthFactorStatusConsumer> status_consumer_;
  FactorsStatusMap factors_state_;
};

TEST_F(AuthHubTestVector, InvalidPurposeOnLoginScreen) {
  AuthHub::Get()->InitializeForMode(AuthHubMode::kLoginScreen);

  EXPECT_CALL(attempt_consumer_, OnUserAuthAttemptRejected());
  AuthHub::Get()->StartAuthentication(account_, AuthPurpose::kWebAuthN,
                                      &attempt_consumer_);
}
TEST_F(AuthHubTestVector, InvalidPurposeInSession) {
  AuthHub::Get()->InitializeForMode(AuthHubMode::kInSession);

  EXPECT_CALL(attempt_consumer_, OnUserAuthAttemptRejected());
  AuthHub::Get()->StartAuthentication(account_, AuthPurpose::kLogin,
                                      &attempt_consumer_);
}

TEST_F(AuthHubTestVector, SingleFactorSuccess) {
  AuthHub::Get()->InitializeForMode(AuthHubMode::kLoginScreen);

  ExpectEngineStart(attempt_);
  ExpectAttemptConfirmation();

  // Make an empty auth factors cache to check if it would be updated.
  factors_cache_->StoreFactorPresenceCache(attempt_, AuthFactorsSet());
  AuthHub::Get()->StartAuthentication(attempt_.account, attempt_.purpose,
                                      &attempt_consumer_);

  ASSERT_NE(engine_observer_, nullptr);

  ConfigureFactorAsAvailable();
  // As factor cache is initially empty, there are no factors.
  EXPECT_TRUE(factors_state_.empty());

  ExpectFactorListUpdate();
  engine_observer_->OnFactorPresenceChecked(kFactor, true);

  EXPECT_TRUE(factors_state_.contains(kFactor));
  EXPECT_EQ(factors_state_[kFactor], AuthFactorState::kFactorReady);

  EXPECT_EQ(AuthFactorsSet({kFactor}),
            factors_cache_->GetExpectedFactorsPresence(attempt_));

  ASSERT_TRUE(engine_usage_.has_value());
  EXPECT_EQ(*engine_usage_, AuthFactorEngine::UsageAllowed::kEnabled);
}

// Test that AuthHub correctly switches mode if there was no
// authentication attempt.
TEST_F(AuthHubTestVector, TestSwitchingMode) {
  // We're on the login screen
  AuthHub::Get()->InitializeForMode(AuthHubMode::kLoginScreen);
  SetupEngineForNextInit();
  // And without starting any authentication attempt,
  // we're getting into session (e.g. via "add new user" flow).
  AuthHub::Get()->InitializeForMode(AuthHubMode::kInSession);
}

// Test that AuthHub correctly switches mode if there is some authentication
// request, but no factor attempt.
TEST_F(AuthHubTestVector, TestSwitchingModeWaitingForAuth) {
  // We're on the login screen
  AuthHub::Get()->InitializeForMode(AuthHubMode::kLoginScreen);
  // We've selected a user pod
  ExpectEngineStart(attempt_);
  ExpectAttemptConfirmation();
  AuthHub::Get()->StartAuthentication(attempt_.account, attempt_.purpose,
                                      &attempt_consumer_);
  // Engine replies that factor is present
  ExpectFactorStateUpdate();
  ConfigureFactorAsAvailable();

  engine_observer_->OnFactorPresenceChecked(kFactor, true);

  // We've logged in in some other way (e.g. via "add new user" flow).
  testing::Mock::VerifyAndClearExpectations(&attempt_consumer_);
  EXPECT_CALL(attempt_consumer_, OnUserAuthAttemptCancelled());
  ExpectEngineStop();
  SetupEngineForNextInit();

  AuthHub::Get()->InitializeForMode(AuthHubMode::kInSession);
  testing::Mock::VerifyAndClearExpectations(&attempt_consumer_);
  // And now we've bringing lock screen.
  attempt_ = AuthAttemptVector{account_, AuthPurpose::kScreenUnlock};
  testing::Mock::VerifyAndClearExpectations(&status_consumer_);
  ExpectEngineStart(attempt_);
  ExpectAttemptConfirmation();
  AuthHub::Get()->StartAuthentication(attempt_.account, attempt_.purpose,
                                      &attempt_consumer_);
  // Engine replies that factor is present
  ExpectFactorStateUpdate();
  ConfigureFactorAsAvailable();
  engine_observer_->OnFactorPresenceChecked(kFactor, true);
}

// Test that AuthHub correctly switches mode if there is some authentication
// request, but before engine have chance to start.
TEST_F(AuthHubTestVector, TestSwitchingModeWhileInitializingEngine) {
  // We're on the login screen
  AuthHub::Get()->InitializeForMode(AuthHubMode::kLoginScreen);
  // We've selected a user pod
  ExpectEngineStart(attempt_);
  ExpectAttemptConfirmation();
  AuthHub::Get()->StartAuthentication(attempt_.account, attempt_.purpose,
                                      &attempt_consumer_);

  // We've logged in in some other way (e.g. via "add new user" flow).
  testing::Mock::VerifyAndClearExpectations(&attempt_consumer_);
  EXPECT_CALL(attempt_consumer_, OnUserAuthAttemptCancelled());
  ExpectEngineStop();
  SetupEngineForNextInit();

  AuthHub::Get()->InitializeForMode(AuthHubMode::kInSession);

  // And now the engine finally starts.
  engine_observer_->OnFactorPresenceChecked(kFactor, true);
}

// Test that AuthHub correctly switches mode if there is some authentication
// request, but engine fails to start in time.
TEST_F(AuthHubTestVector, TestSwitchingModeWithEngineTimeout) {
  // We're on the login screen
  AuthHub::Get()->InitializeForMode(AuthHubMode::kLoginScreen);
  // We've selected a user pod
  ExpectEngineStart(attempt_);
  ExpectAttemptConfirmation();
  AuthHub::Get()->StartAuthentication(attempt_.account, attempt_.purpose,
                                      &attempt_consumer_);

  // We've logged in in some other way (e.g. via "add new user" flow).
  testing::Mock::VerifyAndClearExpectations(&attempt_consumer_);
  EXPECT_CALL(attempt_consumer_, OnUserAuthAttemptCancelled());
  EXPECT_CALL(*engine_, StartFlowTimedOut());
  ExpectEngineStop();

  SetupEngineForNextInit();

  AuthHub::Get()->InitializeForMode(AuthHubMode::kInSession);

  // Engine start times out.
  task_environment_.FastForwardBy(kLongTime);
}

// TODO (b/271248265): add tests for preemption during initialization.

// TODO (b/271248265): add tests for interaction between AuthHub and
// AttemptHandler.

// TODO (b/271248265): add tests that make sure that engines that have failed to
// start are reported as failed to AttemptHandler (for the purpose of UI cache
// tracking).

}  // namespace ash
