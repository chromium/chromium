// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/in_session_auth/in_session_auth_dialog_client.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/auth/cryptohome_pin_engine.h"
#include "chrome/browser/ash/auth/legacy_fingerprint_engine.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_session_intent.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using ::ash::AuthenticationError;
using ::ash::AuthStatusConsumer;
using ::ash::Key;
using ::ash::UserContext;

namespace {

const char kInSessionAuthHelpPageUrl[] =
    "https://support.google.com/chromebook?p=WebAuthn";

InSessionAuthDialogClient* g_auth_dialog_client_instance = nullptr;

}  // namespace

InSessionAuthDialogClient::InSessionAuthDialogClient()
    : auth_performer_(ash::UserDataAuthClient::Get()) {
  ash::WebAuthNDialogController::Get()->SetClient(this);

  DCHECK(!g_auth_dialog_client_instance);
  g_auth_dialog_client_instance = this;
}

InSessionAuthDialogClient::~InSessionAuthDialogClient() {
  ash::WebAuthNDialogController::Get()->SetClient(nullptr);
  DCHECK_EQ(this, g_auth_dialog_client_instance);
  g_auth_dialog_client_instance = nullptr;
}

// static
bool InSessionAuthDialogClient::HasInstance() {
  return !!g_auth_dialog_client_instance;
}

// static
InSessionAuthDialogClient* InSessionAuthDialogClient::Get() {
  DCHECK(g_auth_dialog_client_instance);
  return g_auth_dialog_client_instance;
}

bool InSessionAuthDialogClient::IsFingerprintAuthAvailable(
    const AccountId& account_id) {
  return legacy_fingerprint_engine_->IsFingerprintAvailable(
      ash::LegacyFingerprintEngine::Purpose::kWebAuthn,
      user_context_->GetAccountId());
}

void InSessionAuthDialogClient::StartFingerprintAuthSession(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  legacy_fingerprint_engine_->PrepareLegacyFingerprintFactor(
      std::move(user_context_),
      base::BindOnce(
          &InSessionAuthDialogClient::OnPrepareLegacyFingerprintFactor,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void InSessionAuthDialogClient::OnPrepareLegacyFingerprintFactor(
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  user_context_ = std::move(user_context);

  if (error.has_value()) {
    LOG(ERROR) << "Could not prepare legacy fingerprint auth factor, code: "
               << error->get_cryptohome_code();
    std::move(callback).Run(false);
    return;
  }

  observation_.Observe(ash::UserDataAuthClient::Get());
  std::move(callback).Run(true);
}

void InSessionAuthDialogClient::EndFingerprintAuthSession(
    base::OnceClosure callback) {
  if (legacy_fingerprint_engine_.has_value()) {
    legacy_fingerprint_engine_->TerminateLegacyFingerprintFactor(
        std::move(user_context_),
        base::BindOnce(
            &InSessionAuthDialogClient::OnTerminateLegacyFingerprintFactor,
            weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void InSessionAuthDialogClient::OnTerminateLegacyFingerprintFactor(
    base::OnceClosure callback,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  // Proceed to updating the state and running the callback.
  // We need this regardless of whether an error occurred.
  if (error.has_value()) {
    LOG(ERROR) << "Error terminating legacy fingerprint auth factor, code: "
               << error->get_cryptohome_code();
  }
  user_context_ = std::move(user_context);
  observation_.Reset();
  std::move(callback).Run();
}

void InSessionAuthDialogClient::CheckPinAuthAvailability(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  auto on_pin_availability_checked =
      base::BindOnce(&InSessionAuthDialogClient::OnCheckPinAuthAvailability,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  CHECK(pin_engine_.has_value());
  pin_engine_->IsPinAuthAvailable(
      ash::legacy::CryptohomePinEngine::Purpose::kWebAuthn,
      std::move(user_context_), std::move(on_pin_availability_checked));
}

void InSessionAuthDialogClient::OnCheckPinAuthAvailability(
    base::OnceCallback<void(bool)> callback,
    bool is_pin_auth_available,
    std::unique_ptr<UserContext> user_context) {
  user_context_ = std::move(user_context);
  std::move(callback).Run(is_pin_auth_available);
}

void InSessionAuthDialogClient::StartAuthSession(
    base::OnceCallback<void(bool)> callback) {
  auto* user_manager = user_manager::UserManager::Get();
  const user_manager::User* const user = user_manager->GetActiveUser();
  const bool ephemeral =
      user_manager->IsUserCryptohomeDataEphemeral(user->GetAccountId());
  auto user_context = std::make_unique<UserContext>(*user);

  auto on_auth_session_started =
      base::BindOnce(&InSessionAuthDialogClient::OnAuthSessionStarted,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  auth_performer_.StartAuthSession(std::move(user_context), ephemeral,
                                   ash::AuthSessionIntent::kWebAuthn,
                                   std::move(on_auth_session_started));
}

void InSessionAuthDialogClient::InvalidateAuthSession() {
  if (user_context_) {
    auth_performer_.InvalidateAuthSession(std::move(user_context_),
                                          base::DoNothing());
    pin_engine_.reset();
  }
}

void InSessionAuthDialogClient::AuthenticateUserWithPasswordOrPin(
    const std::string& secret,
    bool authenticated_by_pin,
    base::OnceCallback<void(bool)> callback) {
  // TODO(b/156258540): Pick/validate the correct user.
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(user);
  auto user_context = std::make_unique<UserContext>(*user);
  Key key(Key::KEY_TYPE_PASSWORD_PLAIN, std::string(), secret);
  user_context->SetIsUsingPin(authenticated_by_pin);
  user_context->SetSyncPasswordData(password_manager::PasswordHashData(
      user->GetAccountId().GetUserEmail(), base::UTF8ToUTF16(secret),
      false /*force_update*/));
  if (user->GetAccountId().GetAccountType() == AccountType::ACTIVE_DIRECTORY) {
    LOG(FATAL) << "Incorrect Active Directory user type "
               << user_context->GetUserType();
  }

  DCHECK(!pending_auth_state_);
  pending_auth_state_.emplace(std::move(callback));

  if (!authenticated_by_pin) {
    // TODO(yichengli): If user type is SUPERVISED, use supervised
    // authenticator?
    user_context->SetKey(std::move(key));
    AuthenticateWithPassword(std::move(user_context), secret);
    return;
  }

  pin_engine_->Authenticate(
      cryptohome::RawPin(secret), std::move(user_context_),
      base::BindOnce(&InSessionAuthDialogClient::OnAuthVerified,
                     weak_factory_.GetWeakPtr(),
                     /*authenticated_by_password=*/false));
}

void InSessionAuthDialogClient::OnPinAttemptDone(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (!error.has_value()) {
    OnAuthSuccess(std::move(*user_context));
  } else {
    // Do not try submitting as password.
    if (pending_auth_state_) {
      std::move(pending_auth_state_->callback).Run(false);
      pending_auth_state_.reset();
    }
  }
}

void InSessionAuthDialogClient::AuthenticateWithPassword(
    std::unique_ptr<UserContext> user_context,
    const std::string& password) {
  // Check that we have a valid `user_context_`, provided by a prior
  // `StartAuthSession`, this also nicely asserts that we are not waiting
  // on other UserDataAuth dbus calls that involve the auth_session_id stored
  // in this `user_context`.
  CHECK(user_context_);

  const auto* password_factor =
      user_context_->GetAuthFactorsData().FindAnyPasswordFactor();
  if (!password_factor) {
    LOG(ERROR) << "Could not find password key";
    std::move(pending_auth_state_->callback).Run(false);
    return;
  }

  auto on_authenticated = base::BindOnce(
      &InSessionAuthDialogClient::OnAuthVerified, weak_factory_.GetWeakPtr(),
      /*authenticated_by_password=*/true);

  auth_performer_.AuthenticateWithPassword(*(password_factor->ref().label()),
                                           password, std::move(user_context_),
                                           std::move(on_authenticated));
}

void InSessionAuthDialogClient::OnAuthSessionStarted(
    base::OnceCallback<void(bool)> callback,
    bool user_exists,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to start auth session, code "
               << error->get_cryptohome_code();
    std::move(callback).Run(false);
    return;
  }

  if (!user_exists) {
    LOG(ERROR) << "User does not exist";
    // Invalidate the auth session that started, do not leave orphans behind.
    auth_performer_.InvalidateAuthSession(std::move(user_context),
                                          base::DoNothing());
    std::move(callback).Run(false);
    return;
  }

  // Take temporary ownership of user_context to pass on later.
  user_context_ = std::move(user_context);
  pin_engine_.emplace(&auth_performer_);
  legacy_fingerprint_engine_.emplace(&auth_performer_);
  std::move(callback).Run(true);
}

void InSessionAuthDialogClient::OnAuthVerified(
    bool authenticated_by_password,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  // Take back ownership of user_context for future auth attempts.
  user_context_ = std::move(user_context);

  if (error.has_value()) {
    LOG(ERROR) << "Failed to authenticate, code "
               << error->get_cryptohome_code();
    std::move(pending_auth_state_->callback).Run(false);
  } else {
    // TODO(b:241256423): Tell cryptohome to release WebAuthN secret.
    if (authenticated_by_password)
      OnPasswordAuthSuccess(*user_context_);
    std::move(pending_auth_state_->callback).Run(true);
  }

  pending_auth_state_.reset();
}

void InSessionAuthDialogClient::OnPasswordAuthSuccess(
    const UserContext& user_context) {
  ash::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      ash::quick_unlock::QuickUnlockFactory::GetForAccountId(
          user_context.GetAccountId());
  if (quick_unlock_storage)
    quick_unlock_storage->MarkStrongAuth();
}

void InSessionAuthDialogClient::AuthenticateUserWithFingerprint(
    base::OnceCallback<void(bool, ash::FingerprintState)> callback) {
  DCHECK(!fingerprint_scan_done_callback_);
  fingerprint_scan_done_callback_ = std::move(callback);
}

void InSessionAuthDialogClient::OnFingerprintScan(
    const ::user_data_auth::FingerprintScanResult& result) {
  if (!fingerprint_scan_done_callback_)
    return;

  switch (result) {
    case user_data_auth::FINGERPRINT_SCAN_RESULT_SUCCESS:
      std::move(fingerprint_scan_done_callback_)
          .Run(true, ash::FingerprintState::AVAILABLE_DEFAULT);
      break;
    case user_data_auth::FINGERPRINT_SCAN_RESULT_RETRY:
      std::move(fingerprint_scan_done_callback_)
          .Run(false, ash::FingerprintState::AVAILABLE_DEFAULT);
      break;
    case user_data_auth::FINGERPRINT_SCAN_RESULT_LOCKOUT:
      std::move(fingerprint_scan_done_callback_)
          .Run(false, ash::FingerprintState::DISABLED_FROM_ATTEMPTS);
      break;
    case user_data_auth::FINGERPRINT_SCAN_RESULT_FATAL_ERROR:
      std::move(fingerprint_scan_done_callback_)
          .Run(false, ash::FingerprintState::UNAVAILABLE);
      break;
    default:
      std::move(fingerprint_scan_done_callback_)
          .Run(false, ash::FingerprintState::UNAVAILABLE);
  }
}

aura::Window* InSessionAuthDialogClient::OpenInSessionAuthHelpPage() const {
  // TODO(b/156258540): Use the profile of the source browser window.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  // Create new browser window because the auth dialog is a child of the
  // existing one.
  NavigateParams params(profile, GURL(kInSessionAuthHelpPageUrl),
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.trusted_source = true;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.user_gesture = true;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  Navigate(&params);

  return params.browser->window()->GetNativeWindow();
}

// AuthStatusConsumer:
void InSessionAuthDialogClient::OnAuthFailure(const ash::AuthFailure& error) {
  if (pending_auth_state_) {
    std::move(pending_auth_state_->callback).Run(false);
    pending_auth_state_.reset();
  }
}

void InSessionAuthDialogClient::OnAuthSuccess(const UserContext& user_context) {
  if (pending_auth_state_) {
    std::move(pending_auth_state_->callback).Run(true);
    pending_auth_state_.reset();
  }
}

InSessionAuthDialogClient::AuthState::AuthState(
    base::OnceCallback<void(bool)> callback)
    : callback(std::move(callback)) {}

InSessionAuthDialogClient::AuthState::~AuthState() = default;
