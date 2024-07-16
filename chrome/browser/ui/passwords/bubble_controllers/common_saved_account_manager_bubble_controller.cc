// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/common_saved_account_manager_bubble_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {

std::u16string GetPrimaryAccountEmailFromProfile(Profile* profile) {
  if (!profile) {
    return std::u16string();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return std::u16string();
  }
  return base::UTF8ToUTF16(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email);
}

}  // namespace

CommonSavedAccountManagerBubbleController::
    CommonSavedAccountManagerBubbleController(
        base::WeakPtr<PasswordsModelDelegate> delegate,
        DisplayReason display_reason,
        password_manager::metrics_util::UIDisplayDisposition
            display_disposition)
    : PasswordBubbleControllerBase(delegate, display_disposition),
      display_disposition_(display_disposition),
      dismissal_reason_(password_manager::metrics_util::NO_DIRECT_INTERACTION) {
  state_ = delegate_->GetState();
  CHECK(state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
        state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
        state_ == password_manager::ui::GENERATED_PASSWORD_CONFIRMATION_STATE);
  origin_ = delegate_->GetOrigin();
  pending_password_ = delegate_->GetPendingPassword();
}

CommonSavedAccountManagerBubbleController::
    ~CommonSavedAccountManagerBubbleController() = default;

void CommonSavedAccountManagerBubbleController::OnNoThanksClicked() {
  CHECK(state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
        state_ == password_manager::ui::GENERATED_PASSWORD_CONFIRMATION_STATE);
  dismissal_reason_ = password_manager::metrics_util::CLICKED_CANCEL;
  if (delegate_) {
    delegate_->OnNopeUpdateClicked();
  }
}

void CommonSavedAccountManagerBubbleController::OnCredentialEdited(
    std::u16string new_username,
    std::u16string new_password) {
  CHECK(state_ == password_manager::ui::PENDING_PASSWORD_STATE ||
        state_ == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
        state_ == password_manager::ui::GENERATED_PASSWORD_CONFIRMATION_STATE);
  pending_password_.username_value = std::move(new_username);
  pending_password_.password_value = std::move(new_password);
}

void CommonSavedAccountManagerBubbleController::
    OnGooglePasswordManagerLinkClicked(
        password_manager::ManagePasswordsReferrer refferer) {
  if (delegate_) {
    delegate_->NavigateToPasswordManagerSettingsPage(refferer);
  }
}

std::u16string
CommonSavedAccountManagerBubbleController::GetPrimaryAccountEmail() {
  Profile* profile = GetProfile();
  return GetPrimaryAccountEmailFromProfile(profile);
}

void CommonSavedAccountManagerBubbleController::OnUserAuthenticationCompleted(
    base::OnceCallback<void(bool)> completion,
    bool authentication_result) {
  if (authentication_result) {
    delegate_->OnPasswordsRevealed();
  }
  std::move(completion).Run(authentication_result);
}

url::Origin CommonSavedAccountManagerBubbleController::GetOrigin() const {
  return origin_;
}

password_manager::ui::State
CommonSavedAccountManagerBubbleController::GetState() const {
  return state_;
}

const password_manager::PasswordForm&
CommonSavedAccountManagerBubbleController::GetPendingPassword() const {
  return pending_password_;
}

password_manager::metrics_util::UIDisplayDisposition
CommonSavedAccountManagerBubbleController::GetDisplayDisposition() const {
  return display_disposition_;
}

password_manager::metrics_util::UIDismissalReason
CommonSavedAccountManagerBubbleController::GetDismissalReason() const {
  return dismissal_reason_;
}

void CommonSavedAccountManagerBubbleController::SetDismissalReason(
    password_manager::metrics_util::UIDismissalReason reason) {
  dismissal_reason_ = reason;
}
