// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_hub_mode_lifecycle.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_impl.h"
#include "chromeos/ash/components/osauth/impl/auth_parts_impl.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_attempt_consumer.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_engine.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_engine_factory.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_status_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

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
    parts_ = AuthPartsImpl::CreateTestInstance();
    parts_->SetAuthHub(std::make_unique<AuthHubImpl>());

    auto factory = std::make_unique<StrictMock<MockAuthFactorEngineFactory>>();
    auto engine = std::make_unique<StrictMock<MockAuthFactorEngine>>();

    engine_ = engine.get();

    EXPECT_CALL(*factory, GetFactor()).WillRepeatedly(Return(kFactor));
    EXPECT_CALL(*engine, GetFactor()).WillRepeatedly(Return(kFactor));
    EXPECT_CALL(*engine, InitializeCommon(_))
        .Times(AnyNumber())
        .WillRepeatedly(RunOnceCallback<0>(kFactor));
    EXPECT_CALL(*engine, ShutdownCommon(_))
        .Times(AnyNumber())
        .WillRepeatedly(RunOnceCallback<0>(kFactor));

    EXPECT_CALL(*factory, CreateEngine(_))
        .WillOnce(Return(ByMove(std::move(engine))));
    parts_->RegisterEngineFactory(std::move(factory));
  }

  void ExpectEngineStart(AuthAttemptVector vector) {
    EXPECT_CALL(*engine_,
                StartAuthFlow(Eq(vector.account), Eq(vector.purpose), _))
        .WillOnce(SaveArg<2>(&engine_observer_));
    EXPECT_CALL(*engine_, UpdateObserver(_))
        .Times(AnyNumber())
        .WillRepeatedly(SaveArg<0>(&engine_observer_));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<AuthPartsImpl> parts_;

  base::raw_ptr<MockAuthFactorEngine> engine_;
  base::raw_ptr<AuthFactorEngine::FactorEngineObserver> engine_observer_ =
      nullptr;
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
    EXPECT_CALL(attempt_consumer_, OnUserAuthAttemptConfirmed(_, _))
        .WillOnce(Invoke([&](AuthHubConnector* connector,
                             raw_ptr<AuthFactorStatusConsumer>& out_consumer) {
          connector_ = connector;
          out_consumer = &status_consumer_;
        }));
  }

  AccountId account_;
  AuthAttemptVector attempt_;
  base::raw_ptr<AuthHubConnector> connector_ = nullptr;
  StrictMock<MockAuthAttemptConsumer> attempt_consumer_;
  StrictMock<MockAuthFactorStatusConsumer> status_consumer_;
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

  AuthHub::Get()->StartAuthentication(attempt_.account, attempt_.purpose,
                                      &attempt_consumer_);

  ASSERT_NE(engine_observer_, nullptr);
  engine_observer_->OnFactorPresenceChecked(kFactor, true);
}

// TODO (b/271248265): add tests for preemption during initialization.

}  // namespace ash
