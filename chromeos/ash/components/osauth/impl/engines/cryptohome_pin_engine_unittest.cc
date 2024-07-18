// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/engines/cryptohome_pin_engine.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/osauth/impl/cryptohome_core_impl.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/test_support/engine_test_util.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_engine.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/known_user.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::base::test::TestFuture;
using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::IsFalse;
using ::testing::IsTrue;

class CryptohomePinEngineTest : public EngineTestBase {
 protected:
  CryptohomePinEngineTest()
      : engine_impl_(core_, &prefs_), engine_(&engine_impl_) {}

  // Define basic expectations for StartAuthSession and ListAuthFactors calls.
  // These will create a minimal session and indicate that only one pin
  // auth factor is available.
  void ExpectStartAndList() {
    EXPECT_CALL(mock_udac_, StartAuthSession(_, _))
        .WillOnce([](auto&&, auto&& callback) {
          user_data_auth::StartAuthSessionReply reply;
          reply.set_user_exists(true);
          reply.set_auth_session_id("fake id");
          auto* factor = reply.add_configured_auth_factors_with_status();
          factor->mutable_auth_factor()->set_type(
              user_data_auth::AUTH_FACTOR_TYPE_PIN);
          factor->mutable_auth_factor()->set_label("pin");
          factor->mutable_auth_factor()->mutable_pin_metadata();
          factor->mutable_status_info()->set_time_available_in(0);
          factor->mutable_status_info()->set_time_expiring_in(
              std::numeric_limits<uint64_t>::max());
          std::move(callback).Run(reply);
        });
    EXPECT_CALL(mock_udac_, ListAuthFactors(_, _))
        .WillOnce([](auto&&, auto&& callback) {
          user_data_auth::ListAuthFactorsReply reply;
          auto* factor = reply.add_configured_auth_factors();
          factor->set_type(user_data_auth::AUTH_FACTOR_TYPE_PIN);
          factor->set_label("pin");
          factor->mutable_pin_metadata();
          reply.add_supported_auth_factors(
              user_data_auth::AUTH_FACTOR_TYPE_PIN);
          std::move(callback).Run(reply);
        });
  }

  // The engine under test. The `engine_` pointer variable provides easy access
  // to the public engine API and `engine_impl_` can be used to access the
  // engine-specific functions.
  CryptohomePinEngine engine_impl_;
  raw_ptr<AuthFactorEngine> engine_;
};

TEST_F(CryptohomePinEngineTest, GetFactor) {
  EXPECT_THAT(engine_->GetFactor(), Eq(AshAuthFactor::kCryptohomePin));
}

TEST_F(CryptohomePinEngineTest, StandardSuccessfulAuthenticate) {
  AccountId id = AccountId::FromUserEmail("test@example.com");
  user_manager_.AddUser(id);
  user_manager::KnownUser known_user(&prefs_);
  known_user.SetStringPref(id, prefs::kQuickUnlockPinSalt, "test salt");

  // Initialize the engine.
  TestFuture<AshAuthFactor> init_common;
  engine_->InitializeCommon(init_common.GetCallback());
  EXPECT_THAT(init_common.Get(), Eq(AshAuthFactor::kCryptohomePin));

  // Start the auth flow and enable use of the engine.
  MockAuthFactorEngineObserver observer;
  ExpectStartAndList();
  EXPECT_CALL(observer,
              OnFactorPresenceChecked(AshAuthFactor::kCryptohomePin, true));
  engine_->StartAuthFlow(id, AuthPurpose::kScreenUnlock, &observer);
  engine_->SetUsageAllowed(AuthFactorEngine::UsageAllowed::kEnabled);
  task_environment_.RunUntilIdle();

  // Run the attempt, expect success.
  EXPECT_CALL(mock_udac_, AuthenticateAuthFactor(_, _))
      .WillOnce([](auto&&, auto&& callback) {
        user_data_auth::AuthenticateAuthFactorReply reply;
        reply.set_error(user_data_auth::CRYPTOHOME_ERROR_NOT_SET);
        reply.mutable_auth_properties()->add_authorized_for(
            user_data_auth::AUTH_INTENT_VERIFY_ONLY);
        std::move(callback).Run(reply);
      });
  EXPECT_CALL(observer, OnFactorAttempt(AshAuthFactor::kCryptohomePin));
  EXPECT_CALL(observer,
              OnFactorAttemptResult(AshAuthFactor::kCryptohomePin, true));
  engine_impl_.PerformPinAttempt("raw pin");
  task_environment_.RunUntilIdle();
}

TEST_F(CryptohomePinEngineTest, StandardFailedAuthenticate) {
  AccountId id = AccountId::FromUserEmail("test@example.com");
  user_manager_.AddUser(id);
  user_manager::KnownUser known_user(&prefs_);
  known_user.SetStringPref(id, prefs::kQuickUnlockPinSalt, "test salt");

  // Initialize the engine.
  TestFuture<AshAuthFactor> init_common;
  engine_->InitializeCommon(init_common.GetCallback());
  EXPECT_THAT(init_common.Get(), Eq(AshAuthFactor::kCryptohomePin));

  // Start the auth flow and enable use of the engine.
  MockAuthFactorEngineObserver observer;
  ExpectStartAndList();
  EXPECT_CALL(observer,
              OnFactorPresenceChecked(AshAuthFactor::kCryptohomePin, true));
  engine_->StartAuthFlow(id, AuthPurpose::kScreenUnlock, &observer);
  engine_->SetUsageAllowed(AuthFactorEngine::UsageAllowed::kEnabled);
  task_environment_.RunUntilIdle();

  // Run the attempt, expect failure.
  EXPECT_CALL(mock_udac_, AuthenticateAuthFactor(_, _))
      .WillOnce([](auto&&, auto&& callback) {
        user_data_auth::AuthenticateAuthFactorReply reply;
        reply.set_error(
            user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
        std::move(callback).Run(reply);
      });
  EXPECT_CALL(observer, OnFactorAttempt(AshAuthFactor::kCryptohomePin));
  EXPECT_CALL(observer,
              OnFactorAttemptResult(AshAuthFactor::kCryptohomePin, false));
  engine_impl_.PerformPinAttempt("raw pin");
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace ash
