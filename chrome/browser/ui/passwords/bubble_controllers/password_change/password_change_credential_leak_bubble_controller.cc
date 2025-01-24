// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_credential_leak_bubble_controller.h"

#include <string>

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/url_formatter/elide_url.h"

PasswordChangeCredentialLeakBubbleController::
    PasswordChangeCredentialLeakBubbleController(
        base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          delegate,
          password_manager::metrics_util::UIDisplayDisposition::
              PASSWORD_CHANGE_BUBBLE),
      password_change_delegate_(
          delegate_->GetPasswordChangeDelegate()->AsWeakPtr()) {}

PasswordChangeCredentialLeakBubbleController::
    ~PasswordChangeCredentialLeakBubbleController() {
  OnBubbleClosing();
}

std::u16string PasswordChangeCredentialLeakBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_LEAK_BUBBLE_TITLE);
}

void PasswordChangeCredentialLeakBubbleController::ReportInteractions() {
  // TODO(crbug.com/381053884): Report metrics.
}

std::u16string PasswordChangeCredentialLeakBubbleController::GetDisplayOrigin()
    const {
  return password_change_delegate_->GetDisplayOrigin();
}

void PasswordChangeCredentialLeakBubbleController::
    OnGooglePasswordManagerLinkClicked() {
  if (delegate_) {
    delegate_->NavigateToPasswordManagerSettingsPage(
        password_manager::ManagePasswordsReferrer::kPasswordChangeInfoBubble);
  }
}

std::u16string
PasswordChangeCredentialLeakBubbleController::GetPrimaryAccountEmail() const {
  Profile* profile = GetProfile();
  return base::UTF8ToUTF16(GetDisplayableAccountName(
      SyncServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile)));
}

void PasswordChangeCredentialLeakBubbleController::ChangePassword() {
  password_change_delegate_->StartPasswordChangeFlow();
}

void PasswordChangeCredentialLeakBubbleController::Cancel() {
  CHECK(password_change_delegate_);
  password_change_delegate_->Stop();
}
