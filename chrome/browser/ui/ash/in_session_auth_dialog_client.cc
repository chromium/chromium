// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/in_session_auth_dialog_client.h"

#include <utility>

#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using chromeos::AuthStatusConsumer;
using chromeos::ExtendedAuthenticator;
using chromeos::Key;
using chromeos::UserContext;

namespace {

const char kInSessionAuthHelpPageUrl[] =
    "https://support.google.com/chromebook?p=WebAuthn";

InSessionAuthDialogClient* g_auth_dialog_client_instance = nullptr;

}  // namespace

InSessionAuthDialogClient::InSessionAuthDialogClient() {
  ash::InSessionAuthDialogController::Get()->SetClient(this);

  DCHECK(!g_auth_dialog_client_instance);
  g_auth_dialog_client_instance = this;
}

InSessionAuthDialogClient::~InSessionAuthDialogClient() {
  ash::InSessionAuthDialogController::Get()->SetClient(nullptr);
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
  chromeos::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      chromeos::quick_unlock::QuickUnlockFactory::GetForAccountId(account_id);
  return quick_unlock_storage &&
         quick_unlock_storage->fingerprint_storage()->IsFingerprintAvailable();
}

ExtendedAuthenticator* InSessionAuthDialogClient::GetExtendedAuthenticator() {
  // Lazily allocate |extended_authenticator_| so that tests can inject a fake.
  if (!extended_authenticator_)
    extended_authenticator_ = ExtendedAuthenticator::Create(this);

  return extended_authenticator_.get();
}

void InSessionAuthDialogClient::StartFingerprintAuthSession(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  GetExtendedAuthenticator()->StartFingerprintAuthSession(account_id,
                                                          std::move(callback));
}

void InSessionAuthDialogClient::EndFingerprintAuthSession() {
  DCHECK(extended_authenticator_);
  extended_authenticator_->EndFingerprintAuthSession();
}

void InSessionAuthDialogClient::CheckPinAuthAvailability(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  // PinBackend may be using cryptohome backend or prefs backend.
  chromeos::quick_unlock::PinBackend::GetInstance()->CanAuthenticate(
      account_id, std::move(callback));
}

void InSessionAuthDialogClient::AuthenticateUserWithPasswordOrPin(
    const std::string& password,
    bool authenticated_by_pin,
    base::OnceCallback<void(bool)> callback) {
  // TODO(b/156258540): Pick/validate the correct user.
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(user);
  UserContext user_context(*user);
  user_context.SetKey(
      Key(chromeos::Key::KEY_TYPE_PASSWORD_PLAIN, std::string(), password));
  user_context.SetIsUsingPin(authenticated_by_pin);
  user_context.SetSyncPasswordData(password_manager::PasswordHashData(
      user->GetAccountId().GetUserEmail(), base::UTF8ToUTF16(password),
      false /*force_update*/));
  if (user->GetAccountId().GetAccountType() == AccountType::ACTIVE_DIRECTORY &&
      (user_context.GetUserType() !=
       user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY)) {
    LOG(FATAL) << "Incorrect Active Directory user type "
               << user_context.GetUserType();
  }

  DCHECK(!pending_auth_state_);
  pending_auth_state_.emplace(std::move(callback));

  if (authenticated_by_pin) {
    chromeos::quick_unlock::PinBackend::GetInstance()->TryAuthenticate(
        user_context.GetAccountId(), *user_context.GetKey(),
        base::BindOnce(&InSessionAuthDialogClient::OnPinAttemptDone,
                       weak_factory_.GetWeakPtr(), user_context));
    return;
  }

  // TODO(yichengli): If user type is SUPERVISED, use supervised authenticator?

  AuthenticateWithPassword(user_context);
}

void InSessionAuthDialogClient::OnPinAttemptDone(
    const UserContext& user_context,
    bool success) {
  if (success) {
    // Mark strong auth if this is cryptohome based pin.
    if (chromeos::quick_unlock::PinBackend::GetInstance()->ShouldUseCryptohome(
            user_context.GetAccountId())) {
      chromeos::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
          chromeos::quick_unlock::QuickUnlockFactory::GetForAccountId(
              user_context.GetAccountId());
      if (quick_unlock_storage)
        quick_unlock_storage->MarkStrongAuth();
    }
    OnAuthSuccess(user_context);
  } else {
    // Do not try submitting as password.
    if (pending_auth_state_) {
      std::move(pending_auth_state_->callback).Run(false);
      pending_auth_state_.reset();
    }
  }
}

void InSessionAuthDialogClient::AuthenticateWithPassword(
    const UserContext& user_context) {
  // TODO(crbug.com/1115120): Don't post to UI thread if it turns out to be
  // unnecessary.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ExtendedAuthenticator::AuthenticateToCheck,
          GetExtendedAuthenticator(), user_context,
          base::BindOnce(&InSessionAuthDialogClient::OnPasswordAuthSuccess,
                         weak_factory_.GetWeakPtr(), user_context)));
}

void InSessionAuthDialogClient::OnPasswordAuthSuccess(
    const UserContext& user_context) {
  chromeos::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      chromeos::quick_unlock::QuickUnlockFactory::GetForAccountId(
          user_context.GetAccountId());
  if (quick_unlock_storage)
    quick_unlock_storage->MarkStrongAuth();
}

void InSessionAuthDialogClient::AuthenticateUserWithFingerprint(
    base::OnceCallback<void(bool, ash::FingerprintState)> callback) {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(user);
  UserContext user_context(*user);

  DCHECK(extended_authenticator_);
  extended_authenticator_->AuthenticateWithFingerprint(
      user_context,
      base::BindOnce(&InSessionAuthDialogClient::OnFingerprintAuthDone,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void InSessionAuthDialogClient::OnFingerprintAuthDone(
    base::OnceCallback<void(bool, ash::FingerprintState)> callback,
    user_data_auth::CryptohomeErrorCode error) {
  switch (error) {
    case user_data_auth::CRYPTOHOME_ERROR_NOT_SET:
      std::move(callback).Run(true, ash::FingerprintState::AVAILABLE_DEFAULT);
      break;
    case user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_RETRY_REQUIRED:
      std::move(callback).Run(false, ash::FingerprintState::AVAILABLE_DEFAULT);
      break;
    case user_data_auth::CRYPTOHOME_ERROR_FINGERPRINT_DENIED:
      std::move(callback).Run(false,
                              ash::FingerprintState::DISABLED_FROM_ATTEMPTS);
      break;
    default:
      // Internal error.
      std::move(callback).Run(false, ash::FingerprintState::UNAVAILABLE);
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
void InSessionAuthDialogClient::OnAuthFailure(
    const chromeos::AuthFailure& error) {
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
