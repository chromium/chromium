// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/fake_extended_authenticator.h"

#include "base/notreached.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "components/account_id/account_id.h"

namespace chromeos {

FakeExtendedAuthenticator::FakeExtendedAuthenticator(
    AuthStatusConsumer* consumer,
    const UserContext& expected_user_context)
    : consumer_(consumer), expected_user_context_(expected_user_context) {}

FakeExtendedAuthenticator::~FakeExtendedAuthenticator() = default;

void FakeExtendedAuthenticator::SetConsumer(AuthStatusConsumer* consumer) {
  consumer_ = consumer;
}

void FakeExtendedAuthenticator::AuthenticateToMount(
    const UserContext& context,
    ResultCallback success_callback) {
  if (expected_user_context_ == context) {
    UserContext reported_user_context(context);
    const std::string mount_hash =
        reported_user_context.GetAccountId().GetUserEmail() + "-hash";
    reported_user_context.SetUserIDHash(mount_hash);
    if (success_callback)
      std::move(success_callback).Run(mount_hash);
    OnAuthSuccess(reported_user_context);
    return;
  }

  OnAuthFailure(FAILED_MOUNT,
                AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));
}

void FakeExtendedAuthenticator::AuthenticateToCheck(
    const UserContext& context,
    base::OnceClosure success_callback) {
  if (expected_user_context_ == context) {
    if (success_callback)
      std::move(success_callback).Run();
    OnAuthSuccess(context);
    return;
  }

  OnAuthFailure(FAILED_MOUNT,
                AuthFailure(AuthFailure::UNLOCK_FAILED));
}

void FakeExtendedAuthenticator::StartFingerprintAuthSession(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(expected_user_context_.GetAccountId() == account_id);
}

void FakeExtendedAuthenticator::EndFingerprintAuthSession() {}

void FakeExtendedAuthenticator::AuthenticateWithFingerprint(
    const UserContext& context,
    base::OnceCallback<void(cryptohome::CryptohomeErrorCode)> callback) {
  if (expected_user_context_ == context) {
    std::move(callback).Run(cryptohome::CryptohomeErrorCode::
                                CRYPTOHOME_ERROR_FINGERPRINT_RETRY_REQUIRED);
    return;
  }

  std::move(callback).Run(
      cryptohome::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET);
}

void FakeExtendedAuthenticator::AddKey(const UserContext& context,
                                       const cryptohome::KeyDefinition& key,
                                       bool replace_existing,
                                       base::OnceClosure success_callback) {
  NOTREACHED();
}

void FakeExtendedAuthenticator::RemoveKey(const UserContext& context,
                                          const std::string& key_to_remove,
                                          base::OnceClosure success_callback) {
  NOTREACHED();
}

void FakeExtendedAuthenticator::TransformKeyIfNeeded(
    const UserContext& user_context,
    ContextCallback callback) {
  if (callback)
    std::move(callback).Run(user_context);
}

void FakeExtendedAuthenticator::OnAuthSuccess(const UserContext& context) {
  if (consumer_)
    consumer_->OnAuthSuccess(context);
}

void FakeExtendedAuthenticator::OnAuthFailure(AuthState state,
                                              const AuthFailure& error) {
  if (consumer_)
    consumer_->OnAuthFailure(error);
}

}  // namespace chromeos
