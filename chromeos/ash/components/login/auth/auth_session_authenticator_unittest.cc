// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_session_authenticator.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_metrics_recorder.h"
#include "chromeos/ash/components/login/auth/mock_auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/mock_safe_mode_delegate.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/user_manager/user_directory_integrity_manager.h"
#include "components/user_manager/user_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using cryptohome::KeyData;
using testing::_;
using testing::AllOf;
using testing::AtMost;
using testing::Return;
using user_data_auth::AddAuthFactorReply;
using user_data_auth::AUTH_FACTOR_TYPE_KIOSK;
using user_data_auth::AUTH_FACTOR_TYPE_PASSWORD;
using user_data_auth::AUTH_INTENT_DECRYPT;
using user_data_auth::AUTH_INTENT_VERIFY_ONLY;
using user_data_auth::AUTH_SESSION_FLAGS_EPHEMERAL_USER;
using user_data_auth::AUTH_SESSION_FLAGS_NONE;
using user_data_auth::AuthenticateAuthFactorReply;
using user_data_auth::AuthFactor;
using user_data_auth::CreatePersistentUserReply;
using user_data_auth::ListAuthFactorsReply;
using user_data_auth::PrepareEphemeralVaultReply;
using user_data_auth::PrepareGuestVaultReply;
using user_data_auth::PreparePersistentVaultReply;
using user_data_auth::RemoveReply;
using user_data_auth::StartAuthSessionReply;

namespace ash {

namespace {

constexpr char kEmail[] = "fake-email@example.com";
constexpr char kPassword[] = "pass";
constexpr char kFirstAuthSessionId[] = "123";
constexpr char kSecondAuthSessionId[] = "456";

// Matchers that verify the given cryptohome ...Request protobuf has the
// expected auth_session_id.
MATCHER(WithFirstAuthSessionId, "") {
  return arg.auth_session_id() == kFirstAuthSessionId;
}

MATCHER(WithSecondAuthSessionId, "") {
  return arg.auth_session_id() == kSecondAuthSessionId;
}

// Matcher for `ListAuthFactors` that checks its account_id.
MATCHER(WithAccountId, "") {
  return arg.account_id().account_id() == kEmail;
}

// Matcher for `StartAuthSessionRequest` that checks its account_id, flags and
// intent.
MATCHER_P2(WithAccountIdAndFlags, flags, intent, "") {
  return arg.account_id().account_id() == kEmail &&
         arg.flags() == static_cast<unsigned>(flags) && arg.intent() == intent;
}

// Matcher for `AuthenticateAuthFactorRequest` that verify the key properties.
MATCHER_P(WithPasswordFactorAuth, expected_label, "") {
  if (!arg.auth_input().has_password_input()) {
    return false;
  }
  if (arg.auth_factor_label() != expected_label) {
    return false;
  }

  // Validate the password is already hashed here.
  EXPECT_NE(arg.auth_input().password_input().secret(), "");
  EXPECT_NE(arg.auth_input().password_input().secret(), kPassword);
  return true;
}

// Matcher `AddAuthFactorRequest` that verify the key properties.
MATCHER_P(WithPasswordFactorAdd, expected_label, "") {
  if (!arg.auth_input().has_password_input()) {
    return false;
  }
  if (arg.auth_factor().label() != expected_label) {
    return false;
  }
  if (arg.auth_factor().type() != user_data_auth::AUTH_FACTOR_TYPE_PASSWORD) {
    return false;
  }

  // Validate the password is already hashed here.
  EXPECT_NE(arg.auth_input().password_input().secret(), "");
  EXPECT_NE(arg.auth_input().password_input().secret(), kPassword);
  return true;
}

MATCHER(WithKioskKey, "") {
  return arg.authorization().key().data().type() == KeyData::KEY_TYPE_KIOSK &&
         arg.authorization().key().data().label() ==
             kCryptohomePublicMountLabel;
}

MATCHER(WithKioskFactorAdd, "") {
  if (!arg.auth_input().has_kiosk_input()) {
    return false;
  }
  if (arg.auth_factor().label() != kCryptohomePublicMountLabel) {
    return false;
  }
  if (arg.auth_factor().type() != user_data_auth::AUTH_FACTOR_TYPE_KIOSK) {
    return false;
  }
  return true;
}

MATCHER(WithKioskFactorAuth, "") {
  if (!arg.auth_input().has_kiosk_input()) {
    return false;
  }
  if (arg.auth_factor_label() != kCryptohomePublicMountLabel) {
    return false;
  }
  return true;
}

// GMock action that runs the callback (which is expected to be the second
// argument in the mocked function) with the given reply.
template <typename ReplyType>
auto ReplyWith(const ReplyType& reply) {
  return base::test::RunOnceCallback<1>(reply);
}

StartAuthSessionReply BuildStartReply(const std::string& auth_session_id,
                                      bool user_exists,
                                      const std::vector<AuthFactor>& factors) {
  StartAuthSessionReply reply;
  reply.set_auth_session_id(auth_session_id);
  reply.set_user_exists(user_exists);
  for (const auto& factor : factors) {
    (*reply.add_auth_factors()) = factor;
  }
  return reply;
}

AuthenticateAuthFactorReply BuildAuthenticateFactorSuccessReply() {
  AuthenticateAuthFactorReply reply;
  reply.set_authenticated(true);
  reply.add_authorized_for(user_data_auth::AUTH_INTENT_DECRYPT);
  return reply;
}

AuthenticateAuthFactorReply BuildAuthenticateFactorFailureReply() {
  AuthenticateAuthFactorReply reply;
  reply.set_authenticated(false);
  reply.set_error(user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
  reply.mutable_error_info()->set_primary_action(
      user_data_auth::PRIMARY_INCORRECT_AUTH);
  return reply;
}

AuthFactor PasswordFactor(const std::string& label) {
  AuthFactor factor;
  factor.set_type(AUTH_FACTOR_TYPE_PASSWORD);
  factor.set_label(label);
  factor.mutable_common_metadata();
  factor.mutable_password_metadata();
  return factor;
}

AuthFactor KioskFactor() {
  AuthFactor factor;
  factor.set_type(AUTH_FACTOR_TYPE_KIOSK);
  factor.set_label(kCryptohomePublicMountLabel);
  factor.mutable_common_metadata();
  factor.mutable_kiosk_metadata();
  return factor;
}

}  // namespace

class AuthSessionAuthenticatorTest : public ::testing::Test {
 protected:
  const AccountId kAccountId = AccountId::FromUserEmail(kEmail);

  AuthSessionAuthenticatorTest() {
    auth_metrics_recorder_ = AuthMetricsRecorder::CreateForTesting();
    auth_metrics_recorder_->OnAuthenticationSurfaceChange(
        AuthMetricsRecorder::AuthenticationSurface::kLogin);
    CryptohomeMiscClient::InitializeFake();
    SystemSaltGetter::Initialize();
    UserDataAuthClient::OverrideGlobalInstanceForTesting(&userdataauth_);

    EXPECT_CALL(auth_status_consumer_, OnAuthSuccess(_))
        .Times(AtMost(1))
        .WillOnce([this](const UserContext& user_context) {
          on_auth_success_future_.SetValue(user_context);
        });
    EXPECT_CALL(auth_status_consumer_, OnAuthFailure(_))
        .Times(AtMost(1))
        .WillOnce([this](const AuthFailure& error) {
          on_auth_failure_future_.SetValue(error);
        });
    EXPECT_CALL(auth_status_consumer_, OnPasswordChangeDetectedLegacy(_))
        .Times(AtMost(1))
        .WillOnce([this](const UserContext& user_context) {
          on_password_change_detected_future_.SetValue(user_context);
        });
    EXPECT_CALL(auth_status_consumer_, OnPasswordChangeDetected(_))
        .Times(AtMost(1))
        .WillOnce([this](std::unique_ptr<UserContext> user_context) {
          on_password_change_detected_future_.SetValue(*user_context);
        });
    EXPECT_CALL(auth_status_consumer_, OnOffTheRecordAuthSuccess())
        .Times(AtMost(1))
        .WillOnce([this]() {
          on_off_the_record_auth_success_future_.SetValue(true);
        });
  }

  ~AuthSessionAuthenticatorTest() override {
    SystemSaltGetter::Shutdown();
    CryptohomeMiscClient::Shutdown();
  }

  void RegisterPrefs() {
    user_manager::UserDirectoryIntegrityManager::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  void CreateAuthenticator(bool is_ephemeral_mount_enforced) {
    auto owned_safe_mode_delegate = std::make_unique<MockSafeModeDelegate>();
    safe_mode_delegate_ = owned_safe_mode_delegate.get();
    ON_CALL(*safe_mode_delegate_, IsSafeMode).WillByDefault(Return(false));
    RegisterPrefs();
    authenticator_ = base::MakeRefCounted<AuthSessionAuthenticator>(
        &auth_status_consumer_, std::move(owned_safe_mode_delegate),
        /*user_recorder=*/base::DoNothing(), is_ephemeral_mount_enforced,
        &local_state_);
  }

  MockUserDataAuthClient& userdataauth() { return userdataauth_; }

  Authenticator& authenticator() {
    DCHECK(authenticator_);
    return *authenticator_;
  }

  base::test::TestFuture<UserContext>& on_auth_success_future() {
    return on_auth_success_future_;
  }
  base::test::TestFuture<AuthFailure>& on_auth_failure_future() {
    return on_auth_failure_future_;
  }
  base::test::TestFuture<UserContext>& on_password_change_detected_future() {
    return on_password_change_detected_future_;
  }
  base::test::TestFuture<bool>& on_off_the_record_auth_success_future() {
    return on_off_the_record_auth_success_future_;
  }

  base::test::ScopedFeatureList feature_list_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::TestFuture<UserContext> on_auth_success_future_;
  base::test::TestFuture<AuthFailure> on_auth_failure_future_;
  base::test::TestFuture<UserContext> on_password_change_detected_future_;
  base::test::TestFuture<bool> on_off_the_record_auth_success_future_;
  MockUserDataAuthClient userdataauth_;
  MockAuthStatusConsumer auth_status_consumer_{
      /*quit_closure=*/base::DoNothing()};
  scoped_refptr<AuthSessionAuthenticator> authenticator_;
  // Unowned (points to the object owned by `authenticator_`).
  raw_ptr<MockSafeModeDelegate, ExperimentalAsh> safe_mode_delegate_ = nullptr;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<AuthMetricsRecorder> auth_metrics_recorder_;
};

// Test the `CompleteLogin()` method in the new regular user scenario.
TEST_F(AuthSessionAuthenticatorTest, CompleteLoginRegularNew) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(userdataauth(),
              StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE,
                                                     AUTH_INTENT_DECRYPT),
                               _))
      .WillOnce(ReplyWith(BuildStartReply(kFirstAuthSessionId,
                                          /*user_exists=*/false,
                                          /*factors=*/{})));
  EXPECT_CALL(userdataauth(), CreatePersistentUser(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(CreatePersistentUserReply()));
  EXPECT_CALL(userdataauth(),
              PreparePersistentVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PreparePersistentVaultReply()));
  EXPECT_CALL(
      userdataauth(),
      AddAuthFactor(AllOf(WithFirstAuthSessionId(),
                          WithPasswordFactorAdd(kCryptohomeGaiaKeyLabel)),
                    _))
      .WillOnce(ReplyWith(AddAuthFactorReply()));
  EXPECT_CALL(userdataauth(), ListAuthFactors(WithAccountId(), _))
      .WillOnce(ReplyWith(ListAuthFactorsReply()));

  // Act.
  authenticator().CompleteLogin(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `CompleteLogin()` method in the existing regular user scenario.
TEST_F(AuthSessionAuthenticatorTest, CompleteLoginRegularExisting) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(userdataauth(),
              StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE,
                                                     AUTH_INTENT_DECRYPT),
                               _))
      .WillOnce(ReplyWith(BuildStartReply(
          kFirstAuthSessionId, /*user_exists=*/true,
          /*factors=*/{PasswordFactor(kCryptohomeGaiaKeyLabel)})));
  EXPECT_CALL(userdataauth(),
              AuthenticateAuthFactor(
                  AllOf(WithFirstAuthSessionId(),
                        WithPasswordFactorAuth(kCryptohomeGaiaKeyLabel)),
                  _))
      .WillOnce(ReplyWith(BuildAuthenticateFactorSuccessReply()));
  EXPECT_CALL(userdataauth(),
              PreparePersistentVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PreparePersistentVaultReply()));

  // Act.
  authenticator().CompleteLogin(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}
// Test the `CompleteLogin()` method in the password change scenario for the
// existing regular user.
TEST_F(AuthSessionAuthenticatorTest,
       CompleteLoginRegularExistingPasswordChange) {
  feature_list_.InitAndDisableFeature(ash::features::kCryptohomeRecovery);
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(userdataauth(),
              StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE,
                                                     AUTH_INTENT_DECRYPT),
                               _))
      .WillOnce(ReplyWith(BuildStartReply(
          kFirstAuthSessionId,
          /*user_exists=*/true,
          /*factors=*/{PasswordFactor(kCryptohomeGaiaKeyLabel)})));
  // Set up the cryptohome authentication request to return a failure, since
  // we're simulating the case when it only knows about the old password.
  EXPECT_CALL(userdataauth(),
              AuthenticateAuthFactor(
                  AllOf(WithFirstAuthSessionId(),
                        WithPasswordFactorAuth(kCryptohomeGaiaKeyLabel)),
                  _))
      .WillOnce(ReplyWith(BuildAuthenticateFactorFailureReply()));

  // Act.
  authenticator().CompleteLogin(std::move(user_context));
  const UserContext got_user_context =
      on_password_change_detected_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `CompleteLogin()` method in the password change scenario for the
// existing regular user.
TEST_F(AuthSessionAuthenticatorTest,
       CompleteLoginRegularExistingPasswordChangeRecoveryEnabled) {
  feature_list_.InitAndEnableFeature(ash::features::kCryptohomeRecovery);
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(userdataauth(),
              StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE,
                                                     AUTH_INTENT_DECRYPT),
                               _))
      .WillOnce(ReplyWith(BuildStartReply(
          kFirstAuthSessionId,
          /*user_exists=*/true,
          /*factors=*/{PasswordFactor(kCryptohomeGaiaKeyLabel)})));
  // Set up the cryptohome authentication request to return a failure, since
  // we're simulating the case when it only knows about the old password.
  EXPECT_CALL(userdataauth(),
              AuthenticateAuthFactor(
                  AllOf(WithFirstAuthSessionId(),
                        WithPasswordFactorAuth(kCryptohomeGaiaKeyLabel)),
                  _))
      .WillOnce(ReplyWith(BuildAuthenticateFactorFailureReply()));

  // Act.
  authenticator().CompleteLogin(std::move(user_context));
  const UserContext got_user_context =
      on_password_change_detected_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `CompleteLogin()` method in the ephemeral user scenario.
TEST_F(AuthSessionAuthenticatorTest, CompleteLoginEphemeral) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/true);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(
      userdataauth(),
      StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER,
                                             AUTH_INTENT_DECRYPT),
                       _))
      .WillOnce(ReplyWith(BuildStartReply(kFirstAuthSessionId,
                                          /*user_exists=*/false,
                                          /*factors=*/{})));
  EXPECT_CALL(userdataauth(),
              PrepareEphemeralVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PrepareEphemeralVaultReply()));
  EXPECT_CALL(
      userdataauth(),
      AddAuthFactor(AllOf(WithFirstAuthSessionId(),
                          WithPasswordFactorAdd(kCryptohomeGaiaKeyLabel)),
                    _))
      .WillOnce(ReplyWith(AddAuthFactorReply()));
  EXPECT_CALL(userdataauth(), ListAuthFactors(WithAccountId(), _))
      .WillOnce(ReplyWith(ListAuthFactorsReply()));

  // Act.
  authenticator().CompleteLogin(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `CompleteLogin()` method in the scenario when an ephemeral login is
// requested while having stale persistent data for the same user.
TEST_F(AuthSessionAuthenticatorTest, CompleteLoginEphemeralStaleData) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/true);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  {
    testing::InSequence seq;
    EXPECT_CALL(userdataauth(),
                StartAuthSession(
                    WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER,
                                          AUTH_INTENT_DECRYPT),
                    _))
        .WillOnce(
            ReplyWith(BuildStartReply(kFirstAuthSessionId, /*user_exists=*/true,
                                      /*factors=*/{})))
        .RetiresOnSaturation();
    EXPECT_CALL(userdataauth(), Remove(WithFirstAuthSessionId(), _))
        .WillOnce(ReplyWith(RemoveReply()));
    EXPECT_CALL(userdataauth(),
                StartAuthSession(
                    WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER,
                                          AUTH_INTENT_DECRYPT),
                    _))
        .WillOnce(ReplyWith(BuildStartReply(kSecondAuthSessionId,
                                            /*user_exists=*/false,
                                            /*factors=*/{})));
    EXPECT_CALL(userdataauth(),
                PrepareEphemeralVault(WithSecondAuthSessionId(), _))
        .WillOnce(ReplyWith(PrepareEphemeralVaultReply()));
    EXPECT_CALL(
        userdataauth(),
        AddAuthFactor(AllOf(WithSecondAuthSessionId(),
                            WithPasswordFactorAdd(kCryptohomeGaiaKeyLabel)),
                      _))
        .WillOnce(ReplyWith(AddAuthFactorReply()));
    EXPECT_CALL(userdataauth(), ListAuthFactors(WithAccountId(), _))
        .WillOnce(ReplyWith(ListAuthFactorsReply()));
  }

  // Act.
  authenticator().CompleteLogin(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kSecondAuthSessionId);
}

// Test the `AuthenticateToLogin()` method in the successful scenario.
TEST_F(AuthSessionAuthenticatorTest, AuthenticateToLogin) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(userdataauth(),
              StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE,
                                                     AUTH_INTENT_DECRYPT),
                               _))
      .WillOnce(ReplyWith(BuildStartReply(
          kFirstAuthSessionId,
          /*user_exists=*/true,
          /*factors=*/{PasswordFactor(kCryptohomeGaiaKeyLabel)})));
  EXPECT_CALL(userdataauth(),
              AuthenticateAuthFactor(
                  AllOf(WithFirstAuthSessionId(),
                        WithPasswordFactorAuth(kCryptohomeGaiaKeyLabel)),
                  _))
      .WillOnce(ReplyWith(BuildAuthenticateFactorSuccessReply()));
  EXPECT_CALL(userdataauth(),
              PreparePersistentVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PreparePersistentVaultReply()));

  // Act.
  authenticator().AuthenticateToLogin(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `AuthenticateToLogin()` method in the authentication failure
// scenario.
TEST_F(AuthSessionAuthenticatorTest, AuthenticateToLoginAuthFailure) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(userdataauth(),
              StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE,
                                                     AUTH_INTENT_DECRYPT),
                               _))
      .WillOnce(ReplyWith(BuildStartReply(
          kFirstAuthSessionId, /*user_exists=*/true,
          /*factors=*/{PasswordFactor(kCryptohomeGaiaKeyLabel)})));
  EXPECT_CALL(userdataauth(),
              AuthenticateAuthFactor(
                  AllOf(WithFirstAuthSessionId(),
                        WithPasswordFactorAuth(kCryptohomeGaiaKeyLabel)),
                  _))
      .WillOnce(ReplyWith(BuildAuthenticateFactorFailureReply()));

  // Act.
  authenticator().AuthenticateToLogin(std::move(user_context));
  const AuthFailure auth_failure = on_auth_failure_future().Get();

  // Assert.
  EXPECT_EQ(auth_failure.reason(), AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
}

// Test the `LoginOffTheRecord()` method in the successful scenario.
TEST_F(AuthSessionAuthenticatorTest, LoginOffTheRecord) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  EXPECT_CALL(userdataauth(), PrepareGuestVault(_, _))
      .WillOnce(ReplyWith(PrepareGuestVaultReply()));

  // Act.
  authenticator().LoginOffTheRecord();
  EXPECT_TRUE(on_off_the_record_auth_success_future().Wait());
}

// Test the `LoginAsPublicSession()` method in the successful scenario.
TEST_F(AuthSessionAuthenticatorTest, LoginAsPublicSession) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  UserContext user_context(user_manager::USER_TYPE_PUBLIC_ACCOUNT, kAccountId);
  EXPECT_CALL(
      userdataauth(),
      StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER,
                                             AUTH_INTENT_DECRYPT),
                       _))
      .WillOnce(ReplyWith(BuildStartReply(kFirstAuthSessionId,
                                          /*user_exists=*/false,
                                          /*factors=*/{})));
  EXPECT_CALL(userdataauth(),
              PrepareEphemeralVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PrepareEphemeralVaultReply()));

  // Act.
  authenticator().LoginAsPublicSession(user_context);
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `LoginAsKioskAccount()` method in the scenario when the kiosk
// homedir needs to be created.
TEST_F(AuthSessionAuthenticatorTest, LoginAsKioskAccountNew) {
  // Arrange.

  // Intentionally use `is_ephemeral_mount_enforced` with the opposite value to
  // `ephemeral` flag passed to `LoginAsKioskAccount` function because
  // `is_ephemeral_mount_enforced` does not control ephemeral mount for kiosk
  // accounts anymore.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/true);
  EXPECT_CALL(userdataauth(),
              StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE,
                                                     AUTH_INTENT_DECRYPT),
                               _))
      .WillOnce(
          ReplyWith(BuildStartReply(kFirstAuthSessionId, /*user_exists=*/false,
                                    /*factors=*/{})));
  EXPECT_CALL(userdataauth(), CreatePersistentUser(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(CreatePersistentUserReply()));
  EXPECT_CALL(userdataauth(),
              PreparePersistentVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PreparePersistentVaultReply()));
  EXPECT_CALL(
      userdataauth(),
      AddAuthFactor(AllOf(WithFirstAuthSessionId(), WithKioskFactorAdd()), _))
      .WillOnce(ReplyWith(AddAuthFactorReply()));
  // Note: kiosk flow, unlike other, does not request to list auth factors,
  // as no further editing is assumed.

  // Act.
  authenticator().LoginAsKioskAccount(kAccountId, /*ephemeral=*/false);
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `LoginAsKioskAccount()` method in the scenario when the kiosk
// homedir already exists.
TEST_F(AuthSessionAuthenticatorTest, LoginAsKioskAccountExisting) {
  // Arrange.

  // Intentionally use `is_ephemeral_mount_enforced` with the opposite value to
  // `ephemeral` flag passed to `LoginAsKioskAccount` function because
  // `is_ephemeral_mount_enforced` does not control ephemeral mount for kiosk
  // accounts anymore.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/true);
  KeyData key_data;
  key_data.set_type(KeyData::KEY_TYPE_KIOSK);
  EXPECT_CALL(userdataauth(),
              StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE,
                                                     AUTH_INTENT_DECRYPT),
                               _))
      .WillOnce(
          ReplyWith(BuildStartReply(kFirstAuthSessionId, /*user_exists=*/true,
                                    /*factors=*/{KioskFactor()})));
  EXPECT_CALL(userdataauth(),
              AuthenticateAuthFactor(
                  AllOf(WithFirstAuthSessionId(), WithKioskFactorAuth()), _))
      .WillOnce(ReplyWith(BuildAuthenticateFactorSuccessReply()));
  EXPECT_CALL(userdataauth(),
              PreparePersistentVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PreparePersistentVaultReply()));

  // Act.
  authenticator().LoginAsKioskAccount(kAccountId, /*ephemeral=*/false);
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `LoginAsKioskAccount()` method in the ephemeral kiosk scenario.
TEST_F(AuthSessionAuthenticatorTest, LoginAsKioskAccountEphemeral) {
  // Arrange.

  // Intentionally use `is_ephemeral_mount_enforced` with the opposite value to
  // `ephemeral` flag passed to `LoginAsKioskAccount` function because
  // `is_ephemeral_mount_enforced` does not control ephemeral mount for kiosk
  // accounts anymore.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  EXPECT_CALL(
      userdataauth(),
      StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER,
                                             AUTH_INTENT_DECRYPT),
                       _))
      .WillOnce(ReplyWith(BuildStartReply(kFirstAuthSessionId,
                                          /*user_exists=*/false,
                                          /*factors=*/{})));
  EXPECT_CALL(userdataauth(),
              PrepareEphemeralVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PrepareEphemeralVaultReply()));

  // Act.
  authenticator().LoginAsKioskAccount(kAccountId, /*ephemeral=*/true);
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `LoginAsKioskAccount()` method in the scenario when an ephemeral
// kiosk is requested while having stale persistent data for the same user.
TEST_F(AuthSessionAuthenticatorTest, LoginAsKioskAccountEphemeralStaleData) {
  // Arrange.

  // Intentionally use `is_ephemeral_mount_enforced` with the opposite value to
  // `ephemeral` flag passed to `LoginAsKioskAccount` function because
  // `is_ephemeral_mount_enforced` does not control ephemeral mount for kiosk
  // accounts anymore.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  {
    testing::InSequence seq;
    EXPECT_CALL(userdataauth(),
                StartAuthSession(
                    WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER,
                                          AUTH_INTENT_DECRYPT),
                    _))
        .WillOnce(ReplyWith(BuildStartReply(kFirstAuthSessionId,
                                            /*user_exists=*/true,
                                            /*factors=*/{})))
        .RetiresOnSaturation();
    EXPECT_CALL(userdataauth(), Remove(WithFirstAuthSessionId(), _))
        .WillOnce(ReplyWith(RemoveReply()));
    EXPECT_CALL(userdataauth(),
                StartAuthSession(
                    WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER,
                                          AUTH_INTENT_DECRYPT),
                    _))
        .WillOnce(ReplyWith(BuildStartReply(kSecondAuthSessionId,
                                            /*user_exists=*/false,
                                            /*factors=*/{})));
    EXPECT_CALL(userdataauth(),
                PrepareEphemeralVault(WithSecondAuthSessionId(), _))
        .WillOnce(ReplyWith(PrepareEphemeralVaultReply()));
  }

  // Act.
  authenticator().LoginAsKioskAccount(kAccountId, /*ephemeral=*/true);
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kSecondAuthSessionId);
}

// Test the `AuthenticateToUnlock()` method in the successful scenario.
TEST_F(AuthSessionAuthenticatorTest, AuthenticateToUnlock) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(userdataauth(),
              StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE,
                                                     AUTH_INTENT_VERIFY_ONLY),
                               _))
      .WillOnce(ReplyWith(BuildStartReply(
          kFirstAuthSessionId,
          /*user_exists=*/true,
          /*factors=*/{PasswordFactor(kCryptohomeGaiaKeyLabel)})));
  EXPECT_CALL(userdataauth(),
              AuthenticateAuthFactor(
                  AllOf(WithFirstAuthSessionId(),
                        WithPasswordFactorAuth(kCryptohomeGaiaKeyLabel)),
                  _))
      .WillOnce(ReplyWith(BuildAuthenticateFactorSuccessReply()));

  // Act.
  authenticator().AuthenticateToUnlock(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `AuthenticateToUnlock()` method in the successful scenario for
// ephemeral user with the configured password.
TEST_F(AuthSessionAuthenticatorTest, AuthenticateToUnlockEphemeral) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/true);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(
      userdataauth(),
      StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER,
                                             AUTH_INTENT_VERIFY_ONLY),
                       _))
      .WillOnce(ReplyWith(BuildStartReply(
          kFirstAuthSessionId,
          /*user_exists=*/true,
          /*factors=*/{PasswordFactor(kCryptohomeGaiaKeyLabel)})));
  EXPECT_CALL(userdataauth(),
              AuthenticateAuthFactor(
                  AllOf(WithFirstAuthSessionId(),
                        WithPasswordFactorAuth(kCryptohomeGaiaKeyLabel)),
                  _))
      .WillOnce(ReplyWith(BuildAuthenticateFactorSuccessReply()));

  // Act.
  authenticator().AuthenticateToUnlock(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `AuthenticateToUnlock()` method in the successful scenario for
// Managed Guest Session with the configured password (e.g., Imprivata).
TEST_F(AuthSessionAuthenticatorTest, AuthenticateToUnlockMgs) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_PUBLIC_ACCOUNT, kAccountId);
  EXPECT_CALL(
      userdataauth(),
      StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER,
                                             AUTH_INTENT_VERIFY_ONLY),
                       _))
      .WillOnce(ReplyWith(BuildStartReply(
          kFirstAuthSessionId,
          /*user_exists=*/true,
          /*factors=*/{PasswordFactor(kCryptohomeGaiaKeyLabel)})));
  EXPECT_CALL(userdataauth(),
              AuthenticateAuthFactor(
                  AllOf(WithFirstAuthSessionId(),
                        WithPasswordFactorAuth(kCryptohomeGaiaKeyLabel)),
                  _))
      .WillOnce(ReplyWith(BuildAuthenticateFactorSuccessReply()));

  // Act.
  authenticator().AuthenticateToUnlock(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `AuthenticateToUnlock()` method in the authentication failure
// scenario.
TEST_F(AuthSessionAuthenticatorTest, AuthenticateToUnlockinAuthFailure) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(userdataauth(),
              StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE,
                                                     AUTH_INTENT_VERIFY_ONLY),
                               _))
      .WillOnce(ReplyWith(BuildStartReply(
          kFirstAuthSessionId, /*user_exists=*/true,
          /*factors=*/{PasswordFactor(kCryptohomeGaiaKeyLabel)})));
  EXPECT_CALL(userdataauth(),
              AuthenticateAuthFactor(
                  AllOf(WithFirstAuthSessionId(),
                        WithPasswordFactorAuth(kCryptohomeGaiaKeyLabel)),
                  _))
      .WillOnce(ReplyWith(BuildAuthenticateFactorFailureReply()));

  // Act.
  authenticator().AuthenticateToUnlock(std::move(user_context));
  const AuthFailure auth_failure = on_auth_failure_future().Get();

  // Assert.
  EXPECT_EQ(auth_failure.reason(), AuthFailure::UNLOCK_FAILED);
}

}  // namespace ash
