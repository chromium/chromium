// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/stub_authenticator.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"

namespace ash {

namespace {

// As defined in
// //chromeos/ash/components/dbus/cryptohome/fake_userdataauth_client.cc
static constexpr char kUserIdHashSuffix[] = "-hash";

}  // anonymous namespace

StubAuthenticator::StubAuthenticator(AuthStatusConsumer* consumer,
                                     const UserContext& expected_user_context)
    : Authenticator(consumer),
      expected_user_context_(expected_user_context),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

void StubAuthenticator::CompleteLogin(
    bool ephemeral,
    std::unique_ptr<UserContext> user_context) {
  if (expected_user_context_ != *user_context)
    NOTREACHED_IN_MIGRATION();
  OnAuthSuccess();
}

void StubAuthenticator::AuthenticateToLogin(
    bool ephemeral,
    std::unique_ptr<UserContext> user_context) {
  // Don't compare the entire |expected_user_context_| to |user_context| because
  // during non-online re-auth |user_context| does not have a gaia id.
  if (expected_user_context_.GetAccountId() == user_context->GetAccountId() &&
      (*expected_user_context_.GetKey() == *user_context->GetKey() ||
       *ExpectedUserContextWithTransformedKey().GetKey() ==
           *user_context->GetKey())) {
    switch (auth_action_) {
      case AuthAction::kAuthSuccess:
        task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&StubAuthenticator::OnAuthSuccess, this));
        break;
      case AuthAction::kAuthFailure:
        task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&StubAuthenticator::OnAuthFailure, this,
                                      AuthFailure(failure_reason_)));
        break;
      case AuthAction::kOldEncryption:
        if (user_context->IsForcingDircrypto()) {
          task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&StubAuthenticator::OnOldEncryptionDetected,
                             this));
        } else {
          task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&StubAuthenticator::OnAuthSuccess, this));
        }
    }
    return;
  }
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StubAuthenticator::OnAuthFailure, this,
                                AuthFailure::FromNetworkAuthFailure(error)));
}

void StubAuthenticator::AuthenticateToUnlock(
    bool ephemeral,
    std::unique_ptr<UserContext> user_context) {
  if (expected_user_context_.GetAccountId() == user_context->GetAccountId() &&
      (*expected_user_context_.GetKey() == *user_context->GetKey() ||
       *ExpectedUserContextWithTransformedKey().GetKey() ==
           *user_context->GetKey())) {
    switch (auth_action_) {
      case AuthAction::kAuthFailure:
        task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&StubAuthenticator::OnAuthFailure, this,
                                      AuthFailure(failure_reason_)));
        break;
      case AuthAction::kAuthSuccess:
      case AuthAction::kOldEncryption:
        // The distinction between fields other than AuthAction::kAuthFailure
        // only matter for login.
        task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&StubAuthenticator::OnAuthSuccess, this));
        break;
    }
    return;
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StubAuthenticator::OnAuthFailure, this,
                                AuthFailure(AuthFailure::UNLOCK_FAILED)));
}

void StubAuthenticator::LoginOffTheRecord() {
  consumer_->OnOffTheRecordAuthSuccess();
}

void StubAuthenticator::LoginAsPublicSession(const UserContext& user_context) {
  UserContext logged_in_user_context = user_context;
  logged_in_user_context.SetIsUsingOAuth(false);
  logged_in_user_context.SetMountState(UserContext::MountState::kEphemeral);
  logged_in_user_context.SetUserIDHash(
      logged_in_user_context.GetAccountId().GetUserEmail() + kUserIdHashSuffix);
  logged_in_user_context.GetKey()->Transform(
      Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, "some-salt");
  consumer_->OnAuthSuccess(logged_in_user_context);
}

void StubAuthenticator::LoginAsKioskAccount(
    const AccountId& /* app_account_id */,
    bool /* ephemeral */) {
  LoginAsKioskAccountStub(user_manager::UserType::kKioskApp);
}

void StubAuthenticator::LoginAsWebKioskAccount(
    const AccountId& /* app_account_id */,
    bool /* ephemeral */) {
  LoginAsKioskAccountStub(user_manager::UserType::kWebKioskApp);
}

void StubAuthenticator::LoginAsIwaKioskAccount(
    const AccountId& /* app_account_id */,
    bool /* ephemeral */) {
  LoginAsKioskAccountStub(user_manager::UserType::kKioskIWA);
}

void StubAuthenticator::OnAuthSuccess() {
  // If we want to be more like the real thing, we could save the user ID
  // in AuthenticateToLogin, but there's not much of a point.
  UserContext user_context = ExpectedUserContextWithTransformedKey();
  user_context.SetMountState(UserContext::MountState::kExistingPersistent);
  consumer_->OnAuthSuccess(user_context);
}

void StubAuthenticator::OnAuthFailure(const AuthFailure& failure) {
  consumer_->OnAuthFailure(failure);
}

void StubAuthenticator::LoginAuthenticated(
    std::unique_ptr<UserContext> user_context) {
  consumer_->OnAuthSuccess(*user_context);
}

void StubAuthenticator::SetExpectedCredentials(
    const UserContext& user_context) {
  expected_user_context_ = user_context;
}

StubAuthenticator::~StubAuthenticator() = default;

UserContext StubAuthenticator::ExpectedUserContextWithTransformedKey() const {
  UserContext user_context(expected_user_context_);
  user_context.SetUserIDHash(
      expected_user_context_.GetAccountId().GetUserEmail() + kUserIdHashSuffix);
  user_context.GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                                   "some-salt");
  cryptohome::AuthFactorsSet factors;
  factors.Put(cryptohome::AuthFactorType::kPassword);
  factors.Put(cryptohome::AuthFactorType::kPin);
  factors.Put(cryptohome::AuthFactorType::kRecovery);
  cryptohome::AuthFactorRef ref(cryptohome::AuthFactorType::kPassword,
                                cryptohome::KeyLabel{kCryptohomeGaiaKeyLabel});
  cryptohome::AuthFactor password(ref, cryptohome::AuthFactorCommonMetadata());
  user_context.SetAuthFactorsConfiguration(
      AuthFactorsConfiguration{{password}, factors});
  user_context.SetAuthSessionIds("someauthsessionid", "broadcast");
  user_context.SetSessionLifetime(base::Time::Now() +
                                  cryptohome::kAuthsessionInitialLifetime);
  return user_context;
}

void StubAuthenticator::OnPasswordChangeDetected() {
  consumer_->OnOnlinePasswordUnusable(
      std::make_unique<UserContext>(expected_user_context_), true);
}

void StubAuthenticator::OnOldEncryptionDetected() {
  // The user is expected to finish login using transformed key.
  UserContext user_context = ExpectedUserContextWithTransformedKey();
  consumer_->OnOldEncryptionDetected(
      std::make_unique<UserContext>(user_context),
      has_incomplete_encryption_migration_);
}

void StubAuthenticator::LoginAsKioskAccountStub(
    user_manager::UserType kiosk_type) {
  UserContext user_context(kiosk_type, expected_user_context_.GetAccountId());
  user_context.SetIsUsingOAuth(false);
  user_context.SetMountState(UserContext::MountState::kExistingPersistent);
  user_context.SetUserIDHash(
      expected_user_context_.GetAccountId().GetUserEmail() + kUserIdHashSuffix);
  user_context.GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                                   "some-salt");
  consumer_->OnAuthSuccess(user_context);
}

}  // namespace ash
