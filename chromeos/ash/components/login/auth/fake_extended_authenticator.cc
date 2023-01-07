// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/fake_extended_authenticator.h"

#include "base/notreached.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "components/account_id/account_id.h"

namespace ash {

FakeExtendedAuthenticator::FakeExtendedAuthenticator(
    AuthStatusConsumer* consumer,
    const UserContext& expected_user_context)
    : consumer_(consumer), expected_user_context_(expected_user_context) {}

FakeExtendedAuthenticator::~FakeExtendedAuthenticator() = default;

void FakeExtendedAuthenticator::SetConsumer(AuthStatusConsumer* consumer) {
  consumer_ = consumer;
}

void FakeExtendedAuthenticator::AuthenticateToCheck(
    const UserContext& context,
    base::OnceClosure success_callback) {
  DoAuthenticateToCheck(context, /*unlock_webauthn_secret=*/false,
                        std::move(success_callback));
}

void FakeExtendedAuthenticator::AuthenticateToUnlockWebAuthnSecret(
    const UserContext& context,
    base::OnceClosure success_callback) {
  DoAuthenticateToCheck(context, /*unlock_webauthn_secret=*/true,
                        std::move(success_callback));
}

void FakeExtendedAuthenticator::StartFingerprintAuthSession(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(expected_user_context_.GetAccountId() == account_id);
}

void FakeExtendedAuthenticator::EndFingerprintAuthSession() {}

void FakeExtendedAuthenticator::AuthenticateWithFingerprint(
    const UserContext& context,
    base::OnceCallback<void(::user_data_auth::CryptohomeErrorCode)> callback) {
  if (expected_user_context_ != context) {
    std::move(callback).Run(::user_data_auth::CryptohomeErrorCode::
                                CRYPTOHOME_ERROR_FINGERPRINT_RETRY_REQUIRED);
    return;
  }

  std::move(callback).Run(
      ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET);
}

void FakeExtendedAuthenticator::TransformKeyIfNeeded(
    const UserContext& user_context,
    ContextCallback callback) {
  if (callback)
    std::move(callback).Run(user_context);
}

void FakeExtendedAuthenticator::DoAuthenticateToCheck(
    const UserContext& context,
    bool unlock_webauthn_secret,
    base::OnceClosure success_callback) {
  last_unlock_webauthn_secret_ = unlock_webauthn_secret;
  if (expected_user_context_ == context) {
    if (success_callback)
      std::move(success_callback).Run();
    OnAuthSuccess(context);
    return;
  }

  OnAuthFailure(FAILED_MOUNT, AuthFailure(AuthFailure::UNLOCK_FAILED));
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

}  // namespace ash
