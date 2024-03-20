// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_hub_attempt_handler.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/impl/auth_parts_impl.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_engine.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_status_consumer.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

constexpr AshAuthFactor kFactor = AshAuthFactor::kGaiaPassword;
constexpr AshAuthFactor kSpecialFactor = AshAuthFactor::kLegacyPin;

using base::test::RunOnceCallback;
using testing::_;
using testing::AnyNumber;
using testing::ByMove;
using testing::Eq;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

class MockAttemptHandlerOwner : public AuthHubAttemptHandler::Owner {
 public:
  MockAttemptHandlerOwner() = default;
  ~MockAttemptHandlerOwner() override = default;

  MOCK_METHOD(void,
              UpdateFactorUiCache,
              (const AuthAttemptVector&, AuthFactorsSet),
              (override));
  MOCK_METHOD(void,
              OnAuthenticationSuccess,
              (const AuthAttemptVector&, AshAuthFactor factor),
              (override));
  MOCK_METHOD(void,
              OnFactorAttemptFailed,
              (const AuthAttemptVector&, AshAuthFactor),
              (override));
};

class AuthHubAttemptHandlerTest : public ::testing::Test {
 protected:
  AuthHubAttemptHandlerTest() {
    parts_ = AuthPartsImpl::CreateTestInstance();
    attempt_ = AuthAttemptVector{AccountId::FromUserEmail("user1@example.com"),
                                 AuthPurpose::kLogin};
  }

  ~AuthHubAttemptHandlerTest() override = default;

  void AddAvailableFactor(AshAuthFactor factor) {
    engines_[factor] = std::make_unique<StrictMock<MockAuthFactorEngine>>();
    auto* engine = engines_[factor].get();
    engines_map_[factor] = engine;
    EXPECT_CALL(*engine, SetUsageAllowed(_))
        .Times(AnyNumber())
        .WillRepeatedly(
            Invoke([&, factor](AuthFactorEngine::UsageAllowed usage) {
              engine_usage_[factor] = usage;
            }));
  }

  void InitializeConsumer() {
    EXPECT_CALL(status_consumer_, InitializeUi(_, _))
        .WillOnce(
            Invoke([&](AuthFactorsSet factors, AuthHubConnector* connector) {
              for (auto factor : factors) {
                factors_state_[factor] = AuthFactorState::kCheckingForPresence;
              }
            }));
    handler_->SetConsumer(&status_consumer_);
  }

  void InitializeWithTwoFactors() {
    AddAvailableFactor(kFactor);
    AddAvailableFactor(kSpecialFactor);
    handler_ = std::make_unique<AuthHubAttemptHandler>(
        &owner_, attempt_, engines_map_,
        AuthFactorsSet{kFactor, kSpecialFactor});
    InitializeConsumer();
  }

  void ExpectFactorStateUpdate() {
    EXPECT_CALL(status_consumer_, OnFactorStatusesChanged(_))
        .WillOnce(Invoke([&](FactorsStatusMap update) {
          for (const auto& factor : update) {
            factors_state_[factor.first] = factor.second;
          }
        }))
        .RetiresOnSaturation();
  }

  void ExpectFactorListChange() {
    EXPECT_CALL(status_consumer_, OnFactorListChanged(_))
        .WillOnce(
            Invoke([&](FactorsStatusMap update) { factors_state_ = update; }))
        .RetiresOnSaturation();
  }

  void MarkFactorNotRestricted(AshAuthFactor factor) {
    auto* engine = engines_[factor].get();
    EXPECT_CALL(*engine, IsDisabledByPolicy())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*engine, IsLockedOut())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*engine, IsFactorSpecificRestricted())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  AuthAttemptVector attempt_;
  std::unique_ptr<AuthPartsImpl> parts_;
  StrictMock<MockAttemptHandlerOwner> owner_;
  std::unique_ptr<AuthHubAttemptHandler> handler_;
  AuthEnginesMap engines_map_;
  base::flat_map<AshAuthFactor, std::unique_ptr<MockAuthFactorEngine>> engines_;
  base::flat_map<AshAuthFactor, AuthFactorEngine::UsageAllowed> engine_usage_;
  base::flat_map<AshAuthFactor, AuthFactorState> factors_state_;
  StrictMock<MockAuthFactorStatusConsumer> status_consumer_;
};

TEST_F(AuthHubAttemptHandlerTest, UpdateCacheIfFactorAbsent) {
  InitializeWithTwoFactors();
  MarkFactorNotRestricted(kFactor);
  ExpectFactorListChange();
  AuthFactorsSet factor_cache;
  EXPECT_CALL(owner_, UpdateFactorUiCache(Eq(attempt_), _))
      .WillOnce(SaveArg<1>(&factor_cache));

  handler_->OnFactorsChecked(AuthFactorsSet{kFactor}, AuthFactorsSet{});

  EXPECT_EQ(factors_state_.size(), 1u);
  EXPECT_EQ(factors_state_[kFactor], AuthFactorState::kFactorReady);

  EXPECT_EQ(factor_cache, AuthFactorsSet{kFactor});
}

TEST_F(AuthHubAttemptHandlerTest, NoCacheUpdateIfFactorFailed) {
  InitializeWithTwoFactors();
  MarkFactorNotRestricted(kFactor);
  ExpectFactorStateUpdate();
  handler_->OnFactorsChecked(AuthFactorsSet{kFactor},
                             AuthFactorsSet{kSpecialFactor});
  EXPECT_EQ(factors_state_.size(), 2u);
  EXPECT_EQ(factors_state_[kFactor], AuthFactorState::kFactorReady);
  EXPECT_EQ(factors_state_[kSpecialFactor], AuthFactorState::kEngineError);
}

TEST_F(AuthHubAttemptHandlerTest, UpdateCacheIfFactorAdded) {
  AddAvailableFactor(kFactor);
  MarkFactorNotRestricted(kFactor);
  AddAvailableFactor(kSpecialFactor);
  MarkFactorNotRestricted(kSpecialFactor);
  handler_ = std::make_unique<AuthHubAttemptHandler>(
      &owner_, attempt_, engines_map_, AuthFactorsSet{kFactor});
  InitializeConsumer();

  ExpectFactorListChange();

  AuthFactorsSet factor_cache;
  EXPECT_CALL(owner_, UpdateFactorUiCache(Eq(attempt_), _))
      .WillOnce(SaveArg<1>(&factor_cache));

  handler_->OnFactorsChecked(AuthFactorsSet{kFactor, kSpecialFactor},
                             AuthFactorsSet{});

  EXPECT_EQ(factors_state_.size(), 2u);
  EXPECT_EQ(factors_state_[kFactor], AuthFactorState::kFactorReady);
  EXPECT_EQ(factors_state_[kSpecialFactor], AuthFactorState::kFactorReady);

  EXPECT_EQ(factor_cache, AuthFactorsSet({kFactor, kSpecialFactor}));
}

// TODO (b/271248265): add tests for initial status determination;
// TODO (b/271248265): add tests for status priorities;
// TODO (b/271248265): add tests for status updates;
// TODO (b/271248265): add tests for auth attempts;

}  // namespace ash
