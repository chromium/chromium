// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/failed_password_change_bubble_controller.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

FailedPasswordChangeBubbleController::FailedPasswordChangeBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          delegate,
          password_manager::metrics_util::UIDisplayDisposition::
              PASSWORD_CHANGE_BUBBLE),
      password_change_delegate_(
          delegate_->GetPasswordChangeDelegate()->AsWeakPtr()) {}

FailedPasswordChangeBubbleController::~FailedPasswordChangeBubbleController() {
  OnBubbleClosing();
}

std::u16string FailedPasswordChangeBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FAILED_TITLE);
}

std::u16string FailedPasswordChangeBubbleController::GetBody() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FAILED_BODY);
}

std::u16string FailedPasswordChangeBubbleController::GetAcceptButton() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_FAILED_ACTION);
}

void FailedPasswordChangeBubbleController::ReportInteractions() {
  // TODO(crbug.com/381053884): Report metrics.
}

void FailedPasswordChangeBubbleController::FixManually() {
  password_change_delegate_->OpenPasswordChangeTab();
  FinishPasswordChange();
}

void FailedPasswordChangeBubbleController::FinishPasswordChange() {
  password_change_delegate_->Stop();
}

void FailedPasswordChangeBubbleController::NavigateToPasswordChangeSettings() {
  delegate_->NavigateToPasswordChangeSettings();
}
