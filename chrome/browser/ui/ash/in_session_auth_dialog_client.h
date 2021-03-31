// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_IN_SESSION_AUTH_DIALOG_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_IN_SESSION_AUTH_DIALOG_CLIENT_H_

#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "chromeos/login/auth/user_context.h"

namespace aura {
class Window;
}

class AccountId;

// Handles method calls sent from Ash to ChromeOS.
class InSessionAuthDialogClient : public ash::InSessionAuthDialogClient,
                                  public chromeos::AuthStatusConsumer {
 public:
  using AuthenticateCallback = base::OnceCallback<void(bool)>;

  InSessionAuthDialogClient();
  InSessionAuthDialogClient(const InSessionAuthDialogClient&) = delete;
  InSessionAuthDialogClient& operator=(const InSessionAuthDialogClient&) =
      delete;
  ~InSessionAuthDialogClient() override;

  static bool HasInstance();
  static InSessionAuthDialogClient* Get();

  // ash::InSessionAuthDialogClient:
  void AuthenticateUserWithPasswordOrPin(
      const std::string& password,
      bool authenticated_by_pin,
      AuthenticateCallback callback) override;
  bool IsFingerprintAuthAvailable(const AccountId& account_id) override;
  void StartFingerprintAuthSession(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void EndFingerprintAuthSession() override;
  void CheckPinAuthAvailability(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void AuthenticateUserWithFingerprint(
      base::OnceCallback<void(bool, ash::FingerprintState)> callback) override;
  aura::Window* OpenInSessionAuthHelpPage() const override;

  // AuthStatusConsumer:
  void OnAuthFailure(const chromeos::AuthFailure& error) override;
  void OnAuthSuccess(const chromeos::UserContext& user_context) override;

  // For testing:
  void SetExtendedAuthenticator(
      scoped_refptr<chromeos::ExtendedAuthenticator> extended_authenticator) {
    extended_authenticator_ = std::move(extended_authenticator);
  }

 private:
  // State associated with a pending authentication attempt. Only for Password
  // and PIN, not for fingerprint, since the fingerprint path needs to surface
  // retry status.
  struct AuthState {
    explicit AuthState(base::OnceCallback<void(bool)> callback);
    ~AuthState();

    // Callback that should be executed the authentication result is available.
    base::OnceCallback<void(bool)> callback;
  };

  // Returns a pointer to the ExtendedAuthenticator instance if there is one.
  // Otherwise creates one.
  chromeos::ExtendedAuthenticator* GetExtendedAuthenticator();

  void AuthenticateWithPassword(const chromeos::UserContext& user_context);

  void OnPinAttemptDone(const chromeos::UserContext& user_context,
                        bool success);

  void OnPasswordAuthSuccess(const chromeos::UserContext& user_context);

  void OnFingerprintAuthDone(
      base::OnceCallback<void(bool, ash::FingerprintState)> callback,
      user_data_auth::CryptohomeErrorCode error);

  // Used to authenticate the user to unlock supervised users.
  scoped_refptr<chromeos::ExtendedAuthenticator> extended_authenticator_;

  // State associated with a pending authentication attempt.
  base::Optional<AuthState> pending_auth_state_;

  base::WeakPtrFactory<InSessionAuthDialogClient> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_IN_SESSION_AUTH_DIALOG_CLIENT_H_
