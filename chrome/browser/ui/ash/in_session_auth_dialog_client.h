// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_IN_SESSION_AUTH_DIALOG_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_IN_SESSION_AUTH_DIALOG_CLIENT_H_

#include <memory>
#include <string>

#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/auth/cryptohome_pin_engine.h"
#include "chrome/browser/ui/ash/auth/legacy_fingerprint_engine.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/extended_authenticator.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace aura {
class Window;
}

namespace ash {
class UserContext;
}

class AccountId;

// Handles method calls sent from Ash to ChromeOS.
class InSessionAuthDialogClient
    : public ash::InSessionAuthDialogClient,
      public ash::AuthStatusConsumer,
      public ash::UserDataAuthClient::FingerprintAuthObserver {
 public:
  using AuthenticateCallback = base::OnceCallback<void(bool)>;
  using FingerprintScanDoneCallback =
      base::OnceCallback<void(bool, ash::FingerprintState)>;

  InSessionAuthDialogClient();
  InSessionAuthDialogClient(const InSessionAuthDialogClient&) = delete;
  InSessionAuthDialogClient& operator=(const InSessionAuthDialogClient&) =
      delete;
  ~InSessionAuthDialogClient() override;

  static bool HasInstance();
  static InSessionAuthDialogClient* Get();

  // ash::InSessionAuthDialogClient:
  void StartAuthSession(base::OnceCallback<void(bool)> callback) override;
  void InvalidateAuthSession() override;
  void AuthenticateUserWithPasswordOrPin(
      const std::string& password,
      bool authenticated_by_pin,
      AuthenticateCallback callback) override;
  bool IsFingerprintAuthAvailable(const AccountId& account_id) override;
  void StartFingerprintAuthSession(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void EndFingerprintAuthSession(base::OnceClosure callback) override;
  void CheckPinAuthAvailability(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void AuthenticateUserWithFingerprint(
      base::OnceCallback<void(bool, ash::FingerprintState)> callback) override;
  aura::Window* OpenInSessionAuthHelpPage() const override;

  // AuthStatusConsumer:
  void OnAuthFailure(const ash::AuthFailure& error) override;
  void OnAuthSuccess(const ash::UserContext& user_context) override;

  // UserDataAuthClient::FingerprintAuthObserver
  void OnFingerprintScan(
      const ::user_data_auth::FingerprintScanResult& result) override;
  void OnEnrollScanDone(const ::user_data_auth::FingerprintScanResult& result,
                        bool is_complete,
                        int percent_complete) override {}

  // For testing:
  void SetExtendedAuthenticator(
      scoped_refptr<ash::ExtendedAuthenticator> extended_authenticator) {
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
  ash::ExtendedAuthenticator* GetExtendedAuthenticator();

  // Attempts to authenticate user in `user_context` with the given `password`.
  void AuthenticateWithPassword(std::unique_ptr<ash::UserContext> user_context,
                                const std::string& password);

  // Passed as a callback to `AuthPerformer::StartAuthSession`. Actually
  // initiates the auth attempt.
  void OnAuthSessionStarted(base::OnceCallback<void(bool)> callback,
                            bool user_exists,
                            std::unique_ptr<ash::UserContext> user_context,
                            absl::optional<ash::AuthenticationError> error);

  // Passed as a callback to
  // `LegacyFingerprintEngine::PrepareLegacyFingerprintFactor`.
  void OnPrepareLegacyFingerprintFactor(
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<ash::UserContext> user_context,
      absl::optional<ash::AuthenticationError> error);

  // Passed as a callback to
  // `LegacyFingerprintEngine::TerminateLegacyFingerprintFactor`.
  void OnTerminateLegacyFingerprintFactor(
      base::OnceClosure callback,
      std::unique_ptr<ash::UserContext> user_context,
      absl::optional<ash::AuthenticationError> error);

  // Passed as a callback to `AuthPerformer::AuthenticateWith*`. Checks
  // the result of the authentication operation.
  void OnAuthVerified(bool authenticated_by_password,
                      std::unique_ptr<ash::UserContext> user_context,
                      absl::optional<ash::AuthenticationError> error);

  void OnPinAttemptDone(std::unique_ptr<ash::UserContext> user_context,
                        absl::optional<ash::AuthenticationError> error);

  void OnPasswordAuthSuccess(const ash::UserContext& user_context);

  // Passed as a callback to `CryptohomePinEngine::InPinAuthAvailable`
  // Takes back ownership of the `user_context` that was borrowed by
  // `CryptohomePinEngine` and notifies callers of pin availability
  // status.
  void OnCheckPinAuthAvailability(
      base::OnceCallback<void(bool)> callback,
      bool is_pin_auth_available,
      std::unique_ptr<ash::UserContext> user_context);

  // Used to authenticate the user to unlock supervised users.
  scoped_refptr<ash::ExtendedAuthenticator> extended_authenticator_;

  // State associated with a pending authentication attempt.
  absl::optional<AuthState> pending_auth_state_;

  // Used to start and authenticate auth sessions.
  ash::AuthPerformer auth_performer_;

  absl::optional<ash::legacy::CryptohomePinEngine> pin_engine_;

  absl::optional<ash::LegacyFingerprintEngine> legacy_fingerprint_engine_;

  std::unique_ptr<ash::UserContext> user_context_;

  FingerprintScanDoneCallback fingerprint_scan_done_callback_;

  base::ScopedObservation<ash::UserDataAuthClient,
                          ash::UserDataAuthClient::FingerprintAuthObserver>
      observation_{this};

  base::WeakPtrFactory<InSessionAuthDialogClient> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_IN_SESSION_AUTH_DIALOG_CLIENT_H_
