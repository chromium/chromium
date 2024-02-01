// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/default_store_changed_bubble_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace metrics_util = password_manager::metrics_util;

DefaultStoreChangedBubbleController::DefaultStoreChangedBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(std::move(delegate),
                                   password_manager::metrics_util::
                                       AUTOMATIC_DEFAULT_STORE_CHANGED_BUBBLE) {
  // Since this bubble is only shown when the default password store pref
  // is different than the account storage pref, and the account storage is
  // available, we should align those values.
  if (delegate_) {
    CHECK_EQ(delegate_->GetPasswordFeatureManager()->GetDefaultPasswordStore(),
             password_manager::PasswordForm::Store::kProfileStore);
    delegate_->GetPasswordFeatureManager()->SetDefaultPasswordStore(
        password_manager::PasswordForm::Store::kAccountStore);
  }
}

DefaultStoreChangedBubbleController::~DefaultStoreChangedBubbleController() {
  // Make sure the interactions are reported even if Views didn't notify the
  // controller about the bubble being closed.
  OnBubbleClosing();
}

std::u16string DefaultStoreChangedBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_DEFAULT_STORE_CHANGED_BUBBLE_TITLE);
}

std::u16string DefaultStoreChangedBubbleController::GetBody() const {
  if (!GetProfile()) {
    return l10n_util::GetStringFUTF16(
        IDS_PASSWORD_MANAGER_DEFAULT_STORE_CHANGED_BUBBLE_DESCRIPTION,
        std::u16string());
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  if (!identity_manager) {
    return l10n_util::GetStringFUTF16(
        IDS_PASSWORD_MANAGER_DEFAULT_STORE_CHANGED_BUBBLE_DESCRIPTION,
        std::u16string());
  }

  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_DEFAULT_STORE_CHANGED_BUBBLE_DESCRIPTION,
      base::UTF8ToUTF16(
          identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
              .email));
}

std::u16string DefaultStoreChangedBubbleController::GetContinueButtonText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_DEFAULT_STORE_CHANGED_BUBBLE_CONTINUE_BUTTON);
}

std::u16string DefaultStoreChangedBubbleController::GetGoToSettingsButtonText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_DEFAULT_STORE_CHANGED_BUBBLE_SETTINGS_BUTTON);
}

void DefaultStoreChangedBubbleController::OnContinueButtonClicked() {
  dismissal_reason_ = metrics_util::CLICKED_ACCEPT;
  if (delegate_) {
    delegate_->PromptSaveBubbleAfterDefaultStoreChanged();
  }
}

void DefaultStoreChangedBubbleController::OnNavigateToSettingsButtonClicked() {
  dismissal_reason_ = metrics_util::CLICKED_MANAGE;
  if (delegate_) {
    delegate_->NavigateToPasswordManagerSettingsAccountStoreToggle(
        password_manager::ManagePasswordsReferrer::kDefaultStoreChangedBubble);
  }
}

void DefaultStoreChangedBubbleController::ReportInteractions() {
  password_manager::metrics_util::LogGeneralUIDismissalReason(
      dismissal_reason_);
  base::UmaHistogramBoolean(
      "PasswordBubble.DefaultStoreChangedBubble.ContinueButtonInBubbleClicked",
      dismissal_reason_ == metrics_util::CLICKED_ACCEPT);
}
