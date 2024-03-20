// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_hub_mode_lifecycle.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_mode_lifecycle.h"
#include "chromeos/ash/components/osauth/impl/auth_parts_impl.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_engine.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_engine_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

constexpr base::TimeDelta kLongTime = base::Minutes(1);
constexpr AshAuthFactor kOneFactor = AshAuthFactor::kGaiaPassword;
constexpr AshAuthFactor kAnotherFactor = AshAuthFactor::kLegacyPin;

using base::test::RunOnceCallback;
using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

class MockModeLifecycleOwner : public AuthHubModeLifecycle::Owner {
 public:
  MockModeLifecycleOwner() = default;
  ~MockModeLifecycleOwner() override = default;

  MOCK_METHOD(void, OnReadyForMode, (AuthHubMode, AuthEnginesMap), (override));
  MOCK_METHOD(void, OnExitedMode, (AuthHubMode), (override));
  MOCK_METHOD(void, OnModeShutdown, (), (override));
};

class AuthHubModeLifecycleTest : public ::testing::Test {
 protected:
  AuthHubModeLifecycleTest() { parts_ = AuthPartsImpl::CreateTestInstance(); }

  ~AuthHubModeLifecycleTest() override = default;

  void SetEngineExpectations(MockAuthFactorEngine* engine,
                             AshAuthFactor factor,
                             bool auto_init) {
    EXPECT_CALL(*engine, GetFactor()).WillRepeatedly(Return(factor));
    // Call or store initialization callback.
    if (auto_init) {
      EXPECT_CALL(*engine, InitializeCommon(_))
          .WillOnce(RunOnceCallback<0>(factor));
    } else {
      EXPECT_CALL(*engine, InitializeCommon(_))
          .WillOnce(
              Invoke([&, factor](AuthFactorEngine::CommonInitCallback cb) {
                init_callbacks_[factor] = std::move(cb);
              }));
    }
  }

  void ExpectLoginFactor(AshAuthFactor factor,
                         bool auto_init = true,
                         int times = 1) {
    auto factory = std::make_unique<StrictMock<MockAuthFactorEngineFactory>>();
    EXPECT_CALL(*factory, GetFactor()).WillRepeatedly(Return(factor));
    EXPECT_CALL(*factory, CreateEngine(_))
        .Times(times)
        .WillRepeatedly(Invoke([&, auto_init, factor](AuthHubMode mode) {
          auto engine = std::make_unique<StrictMock<MockAuthFactorEngine>>();
          SetEngineExpectations(engine.get(), factor, auto_init);
          engines_[factor] = engine.get();
          return engine;
        }));
    parts_->RegisterEngineFactory(std::move(factory));
  }

  void ExpectSessionOnlyFactor(AshAuthFactor factor,
                               bool auto_init = true,
                               int times = 1) {
    auto factory = std::make_unique<StrictMock<MockAuthFactorEngineFactory>>();
    EXPECT_CALL(*factory, GetFactor()).WillRepeatedly(Return(factor));
    EXPECT_CALL(*factory, CreateEngine(Eq(AuthHubMode::kInSession)))
        .Times(times)
        .WillRepeatedly(Invoke([&, auto_init, factor](AuthHubMode) {
          auto engine = std::make_unique<StrictMock<MockAuthFactorEngine>>();
          SetEngineExpectations(engine.get(), factor, auto_init);
          engines_[factor] = engine.get();
          return engine;
        }));
    ON_CALL(*factory, CreateEngine(Eq(AuthHubMode::kLoginScreen)))
        .WillByDefault(Invoke([&](AuthHubMode) {
          delete engines_[factor];
          return std::unique_ptr<MockAuthFactorEngine>();
        }));
    parts_->RegisterEngineFactory(std::move(factory));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<AuthPartsImpl> parts_;
  StrictMock<MockModeLifecycleOwner> owner_;
  AuthHubModeLifecycle lifecycle_{&owner_};
  base::flat_map<AshAuthFactor,
                 raw_ptr<MockAuthFactorEngine, AcrossTasksDanglingUntriaged>>
      engines_;
  base::flat_map<AshAuthFactor, AuthFactorEngine::CommonInitCallback>
      init_callbacks_;
};

// Check when Owner is notified during initialization / shutdown.
TEST_F(AuthHubModeLifecycleTest, SingleFactorInitShutdown) {
  ExpectLoginFactor(kOneFactor, /*auto_init=*/false);

  EXPECT_FALSE(lifecycle_.IsReady());
  lifecycle_.SwitchToMode(AuthHubMode::kLoginScreen);

  // Should not notify immediately.
  EXPECT_FALSE(lifecycle_.IsReady());
  Mock::VerifyAndClearExpectations(&owner_);

  AuthEnginesMap engines;
  EXPECT_CALL(owner_, OnReadyForMode(Eq(AuthHubMode::kLoginScreen), _))
      .WillOnce(MoveArg<1>(&engines));

  ASSERT_TRUE(init_callbacks_.contains(kOneFactor));
  std::move(init_callbacks_[kOneFactor]).Run(kOneFactor);

  // Should be notified now.
  EXPECT_TRUE(lifecycle_.IsReady());
  Mock::VerifyAndClearExpectations(&owner_);

  ASSERT_TRUE(engines.contains(kOneFactor));

  AuthFactorEngine::ShutdownCallback callback;
  EXPECT_CALL(*engines_[kOneFactor], ShutdownCommon(_))
      .WillOnce(MoveArg<0>(&callback));

  lifecycle_.SwitchToMode(AuthHubMode::kNone);

  // Should not notify immediately.
  EXPECT_FALSE(lifecycle_.IsReady());
  Mock::VerifyAndClearExpectations(&owner_);

  EXPECT_CALL(owner_, OnExitedMode(Eq(AuthHubMode::kLoginScreen)));
  EXPECT_CALL(owner_, OnModeShutdown());

  std::move(callback).Run(kOneFactor);

  EXPECT_FALSE(lifecycle_.IsReady());
}

// Check owner notifications when shutdown is requested before all
// engines have completed initialization.
TEST_F(AuthHubModeLifecycleTest, SingleFactorShutdownEarly) {
  ExpectLoginFactor(kOneFactor, /*auto_init=*/false);

  EXPECT_FALSE(lifecycle_.IsReady());
  lifecycle_.SwitchToMode(AuthHubMode::kLoginScreen);

  // Should not notify immediately.
  EXPECT_FALSE(lifecycle_.IsReady());
  Mock::VerifyAndClearExpectations(&owner_);

  ASSERT_TRUE(engines_.contains(kOneFactor));

  AuthFactorEngine::ShutdownCallback callback;
  EXPECT_CALL(*engines_[kOneFactor], ShutdownCommon(_))
      .WillOnce(MoveArg<0>(&callback));

  lifecycle_.SwitchToMode(AuthHubMode::kNone);

  // Eventually engine initializes.
  ASSERT_TRUE(init_callbacks_.contains(kOneFactor));
  std::move(init_callbacks_[kOneFactor]).Run(kOneFactor);

  // Should not notify immediately.
  EXPECT_FALSE(lifecycle_.IsReady());
  Mock::VerifyAndClearExpectations(&owner_);

  // No OnReadyForMode / OnExitedMode.
  EXPECT_CALL(owner_, OnModeShutdown());

  ASSERT_FALSE(callback.is_null());
  std::move(callback).Run(kOneFactor);

  EXPECT_FALSE(lifecycle_.IsReady());
}

// Check owner notifications when initialization for another mode is requested
// before all engines have completed initialization.
TEST_F(AuthHubModeLifecycleTest, SingleFactorReInitialization) {
  ExpectLoginFactor(kOneFactor, /*auto_init=*/false, /*times=*/2);

  EXPECT_FALSE(lifecycle_.IsReady());
  lifecycle_.SwitchToMode(AuthHubMode::kLoginScreen);

  // Should not notify immediately.
  EXPECT_FALSE(lifecycle_.IsReady());
  Mock::VerifyAndClearExpectations(&owner_);

  ASSERT_TRUE(engines_.contains(kOneFactor));

  AuthFactorEngine::ShutdownCallback callback;
  EXPECT_CALL(*engines_[kOneFactor], ShutdownCommon(_))
      .WillOnce(MoveArg<0>(&callback));

  lifecycle_.SwitchToMode(AuthHubMode::kInSession);

  // Eventually engine initializes.
  ASSERT_TRUE(init_callbacks_.contains(kOneFactor));
  std::move(init_callbacks_[kOneFactor]).Run(kOneFactor);
  init_callbacks_.erase(kOneFactor);

  // Should not notify immediately.
  EXPECT_FALSE(lifecycle_.IsReady());
  Mock::VerifyAndClearExpectations(&owner_);

  ASSERT_FALSE(callback.is_null());
  std::move(callback).Run(kOneFactor);

  // Should finish shutdown and proceed to initialization for second
  // requested mode.
  AuthEnginesMap engines;
  EXPECT_CALL(owner_, OnReadyForMode(Eq(AuthHubMode::kInSession), _))
      .WillOnce(MoveArg<1>(&engines));

  ASSERT_TRUE(init_callbacks_.contains(kOneFactor));
  std::move(init_callbacks_[kOneFactor]).Run(kOneFactor);

  // Should be notified now.
  EXPECT_TRUE(lifecycle_.IsReady());
  Mock::VerifyAndClearExpectations(&owner_);

  ASSERT_TRUE(engines.contains(kOneFactor));
}

// Check logic when one of the engines takes too long to initialize.
TEST_F(AuthHubModeLifecycleTest, FactorInitializationTimeout) {
  ExpectLoginFactor(kOneFactor, /*auto_init=*/false);
  ExpectLoginFactor(kAnotherFactor, /*auto_init=*/true);

  EXPECT_FALSE(lifecycle_.IsReady());
  lifecycle_.SwitchToMode(AuthHubMode::kLoginScreen);

  // Should not notify immediately.
  EXPECT_FALSE(lifecycle_.IsReady());
  Mock::VerifyAndClearExpectations(&owner_);

  ASSERT_TRUE(engines_.contains(kOneFactor));
  ASSERT_TRUE(engines_.contains(kAnotherFactor));

  EXPECT_CALL(*engines_[kOneFactor], InitializationTimedOut());

  AuthEnginesMap engines;
  EXPECT_CALL(owner_, OnReadyForMode(Eq(AuthHubMode::kLoginScreen), _))
      .WillOnce(MoveArg<1>(&engines));

  // Trigger timeout.
  task_environment_.FastForwardBy(kLongTime);

  // Should be notified now, with only kAnotherFactor factor as "Ready".
  EXPECT_TRUE(lifecycle_.IsReady());
  Mock::VerifyAndClearExpectations(&owner_);

  EXPECT_EQ(engines.size(), 1u);
  ASSERT_TRUE(engines.contains(kAnotherFactor));

  // Set shutdown expectations:
  EXPECT_CALL(*engines_[kOneFactor], ShutdownCommon(_))
      .WillOnce(RunOnceCallback<0>(kOneFactor));
  EXPECT_CALL(*engines_[kAnotherFactor], ShutdownCommon(_))
      .WillOnce(RunOnceCallback<0>(kAnotherFactor));

  EXPECT_CALL(owner_, OnExitedMode(Eq(AuthHubMode::kLoginScreen)));
  EXPECT_CALL(owner_, OnModeShutdown());

  lifecycle_.SwitchToMode(AuthHubMode::kNone);
}

// Check logic when one of the engines takes too long to shut down.
TEST_F(AuthHubModeLifecycleTest, FactorShutdownTimeout) {
  ExpectLoginFactor(kOneFactor);
  ExpectLoginFactor(kAnotherFactor);

  AuthEnginesMap engines;
  EXPECT_CALL(owner_, OnReadyForMode(Eq(AuthHubMode::kLoginScreen), _))
      .WillOnce(MoveArg<1>(&engines));

  lifecycle_.SwitchToMode(AuthHubMode::kLoginScreen);

  EXPECT_TRUE(lifecycle_.IsReady());
  EXPECT_EQ(engines.size(), 2u);

  // Set shutdown expectations:
  AuthFactorEngine::ShutdownCallback callback;
  EXPECT_CALL(*engines_[kOneFactor], ShutdownCommon(_))
      .WillOnce(MoveArg<0>(&callback));

  EXPECT_CALL(*engines_[kAnotherFactor], ShutdownCommon(_))
      .WillOnce(RunOnceCallback<0>(kAnotherFactor));

  lifecycle_.SwitchToMode(AuthHubMode::kNone);

  // Should not notify immediately.
  EXPECT_FALSE(lifecycle_.IsReady());
  Mock::VerifyAndClearExpectations(&owner_);

  EXPECT_CALL(*engines_[kOneFactor], ShutdownTimedOut());
  EXPECT_CALL(owner_, OnExitedMode(Eq(AuthHubMode::kLoginScreen)));
  EXPECT_CALL(owner_, OnModeShutdown());

  // Trigger timeout.
  task_environment_.FastForwardBy(kLongTime);
}

}  // namespace ash
