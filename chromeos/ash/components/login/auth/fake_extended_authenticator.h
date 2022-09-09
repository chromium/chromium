// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_FAKE_EXTENDED_AUTHENTICATOR_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_FAKE_EXTENDED_AUTHENTICATOR_H_

#include "base/component_export.h"
#include "chromeos/ash/components/login/auth/extended_authenticator.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

class AuthFailure;

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
    FakeExtendedAuthenticator : public ExtendedAuthenticator {
 public:
  FakeExtendedAuthenticator(AuthStatusConsumer* consumer,
                            const UserContext& expected_user_context);

  FakeExtendedAuthenticator(const FakeExtendedAuthenticator&) = delete;
  FakeExtendedAuthenticator& operator=(const FakeExtendedAuthenticator&) =
      delete;

  // ExtendedAuthenticator:
  void SetConsumer(AuthStatusConsumer* consumer) override;
  void AuthenticateToCheck(const UserContext& context,
                           base::OnceClosure success_callback) override;
  void AuthenticateToUnlockWebAuthnSecret(
      const UserContext& context,
      base::OnceClosure success_callback) override;
  void StartFingerprintAuthSession(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void EndFingerprintAuthSession() override;
  void AuthenticateWithFingerprint(
      const UserContext& context,
      base::OnceCallback<void(::user_data_auth::CryptohomeErrorCode)> callback)
      override;
  void TransformKeyIfNeeded(const UserContext& user_context,
                            ContextCallback callback) override;

  bool last_unlock_webauthn_secret() const {
    return last_unlock_webauthn_secret_;
  }

 private:
  ~FakeExtendedAuthenticator() override;

  void DoAuthenticateToCheck(const UserContext& context,
                             bool unlock_webauthn_secret,
                             base::OnceClosure success_callback);
  void OnAuthSuccess(const UserContext& context);
  void OnAuthFailure(AuthState state, const AuthFailure& error);

  AuthStatusConsumer* consumer_;

  UserContext expected_user_context_;
  bool last_unlock_webauthn_secret_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_FAKE_EXTENDED_AUTHENTICATOR_H_
