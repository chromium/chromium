// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/engines/prefs_pin_engine.h"

#include "ash/constants/ash_pref_names.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/osauth/impl/prefs.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/test_support/engine_test_util.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_engine.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
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

class PrefsPinEngineTest : public EngineTestBase {
 protected:
  PrefsPinEngineTest() : engine_impl_(core_, prefs_), engine_(&engine_impl_) {
    RegisterPinStoragePrefs(prefs_.registry());
  }

  // Add a salt+pin to the preferences.
  void AddPinToPrefs(const std::string& pin) {
    const std::string salt("ABCDEFGH");
    prefs_.SetString(prefs::kQuickUnlockPinSalt, salt);
    Key key(pin);
    key.Transform(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, salt);
    prefs_.SetString(prefs::kQuickUnlockPinSecret, key.GetSecret());
  }

  // Define basic expectations for StartAuthSession and ListAuthFactors calls.
  // These will create a minimal session and indicate that only pin auth
  // factor support is available.
  void ExpectStartAndList() {
    EXPECT_CALL(mock_udac_, StartAuthSession(_, _))
        .WillOnce([this](auto&&, auto&& callback) {
          user_data_auth::StartAuthSessionReply reply;
          reply.set_user_exists(true);
          reply.set_auth_session_id(kAuthSessionId);
          std::move(callback).Run(reply);
        });
    EXPECT_CALL(mock_udac_, ListAuthFactors(_, _))
        .WillOnce([](auto&&, auto&& callback) {
          user_data_auth::ListAuthFactorsReply reply;
          auto* factor = reply.add_configured_auth_factors();
          factor->set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);
          factor->set_label("password");
          factor->mutable_password_metadata();
          reply.add_supported_auth_factors(
              user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);
          std::move(callback).Run(reply);
        });
  }

  // The engine under test. The `engine_` pointer variable provides easy access
  // to the public engine API and `engine_impl_` can be used to access the
  // engine-specific functions.
  PrefsPinEngine engine_impl_;
  raw_ptr<AuthFactorEngine> engine_;
  const std::string kAuthSessionId = "31415926535";
};

TEST_F(PrefsPinEngineTest, GetFactor) {
  EXPECT_THAT(engine_->GetFactor(), Eq(AshAuthFactor::kLegacyPin));
}

TEST_F(PrefsPinEngineTest, StandardSuccessfulAuthenticate) {
  AccountId id = AccountId::FromUserEmail("test@example.com");
  user_manager_.AddUser(id);
  const std::string pin("12345");
  AddPinToPrefs(pin);

  // Initialize the engine.
  TestFuture<AshAuthFactor> init_common;
  engine_->InitializeCommon(init_common.GetCallback());
  EXPECT_THAT(init_common.Get(), Eq(AshAuthFactor::kLegacyPin));

  // Start the auth flow and enable use of the engine.
  MockAuthFactorEngineObserver observer;
  ExpectStartAndList();
  engine_->StartAuthFlow(id, AuthPurpose::kScreenUnlock, &observer);
  engine_->SetUsageAllowed(AuthFactorEngine::UsageAllowed::kEnabled);

  // Run the attempt, expect success.
  EXPECT_CALL(observer, OnFactorAttempt(AshAuthFactor::kLegacyPin));
  EXPECT_CALL(observer, OnFactorAttemptResult(AshAuthFactor::kLegacyPin, true));
  engine_impl_.PerformPinAttempt(pin);
}

TEST_F(PrefsPinEngineTest, StandardFailedAuthenticate) {
  AccountId id = AccountId::FromUserEmail("test@example.com");
  user_manager_.AddUser(id);
  const std::string pin("12345"), wrong_pin("23456");
  AddPinToPrefs(pin);

  // Initialize the engine.
  TestFuture<AshAuthFactor> init_common;
  engine_->InitializeCommon(init_common.GetCallback());
  EXPECT_THAT(init_common.Get(), Eq(AshAuthFactor::kLegacyPin));

  // Start the auth flow and enable use of the engine.
  MockAuthFactorEngineObserver observer;
  ExpectStartAndList();
  engine_->StartAuthFlow(id, AuthPurpose::kScreenUnlock, &observer);
  engine_->SetUsageAllowed(AuthFactorEngine::UsageAllowed::kEnabled);

  // Run the attempt, expect success.
  EXPECT_CALL(observer, OnFactorAttempt(AshAuthFactor::kLegacyPin));
  EXPECT_CALL(observer,
              OnFactorAttemptResult(AshAuthFactor::kLegacyPin, false));
  engine_impl_.PerformPinAttempt(wrong_pin);
}

TEST_F(PrefsPinEngineTest, FailuresLeadingToLockout) {
  AccountId id = AccountId::FromUserEmail("test@example.com");
  user_manager_.AddUser(id);
  const std::string pin("12345"), wrong_pin("23456");
  AddPinToPrefs(pin);

  // Initialize the engine.
  TestFuture<AshAuthFactor> init_common;
  engine_->InitializeCommon(init_common.GetCallback());
  EXPECT_THAT(init_common.Get(), Eq(AshAuthFactor::kLegacyPin));

  // Start the auth flow and enable use of the engine.
  MockAuthFactorEngineObserver observer;
  ExpectStartAndList();
  engine_->StartAuthFlow(id, AuthPurpose::kScreenUnlock, &observer);
  engine_->SetUsageAllowed(AuthFactorEngine::UsageAllowed::kEnabled);

  // Set up the expected observations for the attempts.
  {
    InSequence s;
    for (int i = 0; i < PrefsPinEngine::kMaximumUnlockAttempts - 1; ++i) {
      EXPECT_CALL(observer, OnFactorAttempt(AshAuthFactor::kLegacyPin));
      EXPECT_CALL(observer,
                  OnFactorAttemptResult(AshAuthFactor::kLegacyPin, false));
    }
    EXPECT_CALL(observer, OnFactorAttempt(AshAuthFactor::kLegacyPin));
    EXPECT_CALL(observer, OnLockoutChanged(AshAuthFactor::kLegacyPin));
    EXPECT_CALL(observer,
                OnFactorAttemptResult(AshAuthFactor::kLegacyPin, false));
  }

  // Run a series of bad attempts until the PIN locks out, then try one final
  // time to make sure that it doesn't work even with the correct PIN.
  for (int i = 0; i < PrefsPinEngine::kMaximumUnlockAttempts; ++i) {
    EXPECT_THAT(engine_->IsLockedOut(), IsFalse());
    engine_impl_.PerformPinAttempt(wrong_pin);
  }
  EXPECT_THAT(engine_->IsLockedOut(), IsTrue());
  engine_impl_.PerformPinAttempt(pin);
}

TEST_F(PrefsPinEngineTest, LockoutClearedAfterAuth) {
  AccountId id = AccountId::FromUserEmail("test@example.com");
  user_manager_.AddUser(id);
  const std::string pin("12345"), wrong_pin("23456");
  AddPinToPrefs(pin);

  // Initialize the engine.
  TestFuture<AshAuthFactor> init_common;
  engine_->InitializeCommon(init_common.GetCallback());
  EXPECT_THAT(init_common.Get(), Eq(AshAuthFactor::kLegacyPin));

  // Start the auth flow and enable use of the engine.
  MockAuthFactorEngineObserver observer;
  ExpectStartAndList();
  engine_->StartAuthFlow(id, AuthPurpose::kScreenUnlock, &observer);
  engine_->SetUsageAllowed(AuthFactorEngine::UsageAllowed::kEnabled);

  // Set up the expected observations for the attempts.
  {
    InSequence s;
    // A bunch of failed PIN attempts, eventually ending in lockout.
    for (int i = 0; i < PrefsPinEngine::kMaximumUnlockAttempts - 1; ++i) {
      EXPECT_CALL(observer, OnFactorAttempt(AshAuthFactor::kLegacyPin));
      EXPECT_CALL(observer,
                  OnFactorAttemptResult(AshAuthFactor::kLegacyPin, false));
    }
    EXPECT_CALL(observer, OnFactorAttempt(AshAuthFactor::kLegacyPin));
    EXPECT_CALL(observer, OnLockoutChanged(AshAuthFactor::kLegacyPin));
    EXPECT_CALL(observer,
                OnFactorAttemptResult(AshAuthFactor::kLegacyPin, false));
    // A successful attempt from after the lockout is cleared.
    EXPECT_CALL(observer, OnFactorAttempt(AshAuthFactor::kLegacyPin));
    EXPECT_CALL(observer,
                OnFactorAttemptResult(AshAuthFactor::kLegacyPin, true));
  }

  // Run a series of bad attempts until the PIN locks out.
  for (int i = 0; i < PrefsPinEngine::kMaximumUnlockAttempts; ++i) {
    EXPECT_THAT(engine_->IsLockedOut(), IsFalse());
    engine_impl_.PerformPinAttempt(wrong_pin);
  }
  EXPECT_THAT(engine_->IsLockedOut(), IsTrue());

  // Signal that a successful auth occurred, which should clear the lockout.
  engine_->OnSuccessfulAuthentiation();
  EXPECT_THAT(engine_->IsLockedOut(), IsFalse());
  engine_impl_.PerformPinAttempt(pin);
}

}  // namespace
}  // namespace ash
