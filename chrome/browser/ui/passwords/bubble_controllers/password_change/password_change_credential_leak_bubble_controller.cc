// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_credential_leak_bubble_controller.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/url_formatter/elide_url.h"

namespace metrics_util = password_manager::metrics_util;

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
  base::UmaHistogramEnumeration(
      "PasswordManager.PasswordChange.LeakDetectionBubble", dismissal_reason_,
      metrics_util::NUM_UI_RESPONSES);
}

std::u16string PasswordChangeCredentialLeakBubbleController::GetDisplayOrigin()
    const {
  return password_change_delegate_->GetDisplayOrigin();
}

void PasswordChangeCredentialLeakBubbleController::
    NavigateToPasswordChangeSettings() {
  dismissal_reason_ = metrics_util::CLICKED_ABOUT_PASSWORD_CHANGE;
  delegate_->NavigateToPasswordChangeSettings();
}

std::u16string
PasswordChangeCredentialLeakBubbleController::GetPrimaryAccountEmail() const {
  Profile* profile = GetProfile();
  return base::UTF8ToUTF16(GetDisplayableAccountName(
      SyncServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile)));
}

void PasswordChangeCredentialLeakBubbleController::ChangePassword() {
  dismissal_reason_ = metrics_util::CLICKED_ACCEPT;
  password_change_delegate_->StartPasswordChangeFlow();
}

void PasswordChangeCredentialLeakBubbleController::Cancel() {
  dismissal_reason_ = metrics_util::CLICKED_CANCEL;
  CHECK(password_change_delegate_);
  password_change_delegate_->Stop();
  delegate_->GetPasswordsLeakDialogDelegate()->OnLeakDialogHidden();
}
