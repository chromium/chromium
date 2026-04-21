// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_factor_configuration_helper.h"

#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kSmartCardLabel[] = "smartcard";

}  // namespace

class AuthFactorConfigurationHelperTest : public ::testing::Test {
 protected:
  AuthFactorConfigurationHelperTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {
    UserDataAuthClient::InitializeFake();
  }

  ~AuthFactorConfigurationHelperTest() override {
    UserDataAuthClient::Shutdown();
  }

  void AddUser(const AccountId& account_id) {
    FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(
        cryptohome::CreateAccountIdentifierFromAccountId(account_id));
  }

  void AddGaiaPassword(const AccountId& account_id) {
    AddAuthFactor(account_id, ::user_data_auth::AUTH_FACTOR_TYPE_PASSWORD,
                  kCryptohomeGaiaKeyLabel);
  }

  void AddLocalPassword(const AccountId& account_id) {
    AddAuthFactor(account_id, ::user_data_auth::AUTH_FACTOR_TYPE_PASSWORD,
                  kCryptohomeLocalPasswordKeyLabel);
  }

  void AddPin(const AccountId& account_id) {
    AddAuthFactor(account_id, ::user_data_auth::AUTH_FACTOR_TYPE_PIN,
                  kCryptohomePinLabel);
  }

  void AddRecovery(const AccountId& account_id) {
    AddAuthFactor(account_id,
                  ::user_data_auth::AUTH_FACTOR_TYPE_CRYPTOHOME_RECOVERY,
                  kCryptohomeRecoveryKeyLabel);
  }

  void AddSmartCard(const AccountId& account_id) {
    AddAuthFactor(account_id, ::user_data_auth::AUTH_FACTOR_TYPE_SMART_CARD,
                  kSmartCardLabel);
  }

  void AddAuthFactor(const AccountId& account_id,
                     ::user_data_auth::AuthFactorType type,
                     const std::string& label) {
    AddUser(account_id);
    ::user_data_auth::AuthFactor factor;
    factor.set_type(type);
    factor.set_label(label);
    ::user_data_auth::AuthInput input;
    FakeUserDataAuthClient::TestApi::Get()->AddAuthFactor(
        cryptohome::CreateAccountIdentifierFromAccountId(account_id), factor,
        input);
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(AuthFactorConfigurationHelperTest, CheckHasAuthFactorsEmpty) {
  const AccountId account_id = AccountId::FromUserEmail("user@example.com");
  AddUser(account_id);

  AuthFactorConfigurationHelper helper;
  base::test::TestFuture<AuthFactorsSet> result;
  helper.CheckHasAuthFactors(account_id, result.GetCallback());

  EXPECT_TRUE(result.Get().empty());
}

TEST_F(AuthFactorConfigurationHelperTest, CheckHasAuthFactorsGaiaPassword) {
  const AccountId account_id = AccountId::FromUserEmail("user@example.com");
  AddGaiaPassword(account_id);

  AuthFactorConfigurationHelper helper;
  base::test::TestFuture<AuthFactorsSet> result;
  helper.CheckHasAuthFactors(account_id, result.GetCallback());

  AuthFactorsSet expected;
  expected.Put(AshAuthFactor::kGaiaPassword);
  EXPECT_EQ(result.Get(), expected);
}

TEST_F(AuthFactorConfigurationHelperTest, CheckHasAuthFactorsLocalPassword) {
  const AccountId account_id = AccountId::FromUserEmail("user@example.com");
  AddLocalPassword(account_id);

  AuthFactorConfigurationHelper helper;
  base::test::TestFuture<AuthFactorsSet> result;
  helper.CheckHasAuthFactors(account_id, result.GetCallback());

  AuthFactorsSet expected;
  expected.Put(AshAuthFactor::kLocalPassword);
  EXPECT_EQ(result.Get(), expected);
}

TEST_F(AuthFactorConfigurationHelperTest, CheckHasAuthFactorsPin) {
  const AccountId account_id = AccountId::FromUserEmail("user@example.com");
  AddPin(account_id);

  AuthFactorConfigurationHelper helper;
  base::test::TestFuture<AuthFactorsSet> result;
  helper.CheckHasAuthFactors(account_id, result.GetCallback());

  AuthFactorsSet expected;
  expected.Put(AshAuthFactor::kCryptohomePin);
  EXPECT_EQ(result.Get(), expected);
}

TEST_F(AuthFactorConfigurationHelperTest, CheckHasAuthFactorsRecovery) {
  const AccountId account_id = AccountId::FromUserEmail("user@example.com");
  AddRecovery(account_id);

  AuthFactorConfigurationHelper helper;
  base::test::TestFuture<AuthFactorsSet> result;
  helper.CheckHasAuthFactors(account_id, result.GetCallback());

  AuthFactorsSet expected;
  expected.Put(AshAuthFactor::kRecovery);
  EXPECT_EQ(result.Get(), expected);
}

TEST_F(AuthFactorConfigurationHelperTest, CheckHasAuthFactorsSmartCard) {
  const AccountId account_id = AccountId::FromUserEmail("user@example.com");
  AddSmartCard(account_id);

  AuthFactorConfigurationHelper helper;
  base::test::TestFuture<AuthFactorsSet> result;
  helper.CheckHasAuthFactors(account_id, result.GetCallback());

  AuthFactorsSet expected;
  expected.Put(AshAuthFactor::kSmartCard);
  EXPECT_EQ(result.Get(), expected);
}

TEST_F(AuthFactorConfigurationHelperTest,
       CheckHasOnlinePasswordAndContinueHasPassword) {
  const AccountId account_id = AccountId::FromUserEmail("user@example.com");
  AddGaiaPassword(account_id);

  AuthFactorConfigurationHelper helper;
  base::test::TestFuture<void> has_password;
  base::test::TestFuture<void> no_password;

  helper.CheckHasOnlinePasswordAndContinue(
      account_id, has_password.GetCallback(), no_password.GetCallback());

  EXPECT_TRUE(has_password.Wait());
  EXPECT_FALSE(no_password.IsReady());
}

TEST_F(AuthFactorConfigurationHelperTest,
       CheckHasOnlinePasswordAndContinueNoPassword) {
  const AccountId account_id = AccountId::FromUserEmail("user@example.com");
  AddUser(account_id);

  AuthFactorConfigurationHelper helper;
  base::test::TestFuture<void> has_password;
  base::test::TestFuture<void> no_password;

  helper.CheckHasOnlinePasswordAndContinue(
      account_id, has_password.GetCallback(), no_password.GetCallback());

  EXPECT_FALSE(has_password.IsReady());
  EXPECT_TRUE(no_password.Wait());
}

}  // namespace ash
