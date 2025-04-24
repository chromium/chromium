// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/otp_during_password_change_bubble_controller.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace metrics_util = password_manager::metrics_util;

OtpDuringPasswordChangeBubbleController::
    OtpDuringPasswordChangeBubbleController(
        base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          delegate,
          password_manager::metrics_util::UIDisplayDisposition::
              PASSWORD_CHANGE_BUBBLE),
      password_change_delegate_(
          delegate_->GetPasswordChangeDelegate()->AsWeakPtr()) {}

OtpDuringPasswordChangeBubbleController::
    ~OtpDuringPasswordChangeBubbleController() {
  OnBubbleClosing();
}

std::u16string OtpDuringPasswordChangeBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_OTP_DURING_PASSWORD_CHANGE_TITLE);
}

std::u16string OtpDuringPasswordChangeBubbleController::GetBody() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_OTP_DURING_PASSWORD_CHANGE_BODY);
}

std::u16string OtpDuringPasswordChangeBubbleController::GetAcceptButtonText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_OTP_DURING_PASSWORD_CHANGE_ACTION);
}

void OtpDuringPasswordChangeBubbleController::ReportInteractions() {
  // TODO(crbug.com/412612384): Add metrics recording.
}

void OtpDuringPasswordChangeBubbleController::FixManually() {
  password_change_delegate_->OpenPasswordChangeTab();
  FinishPasswordChange();
}

void OtpDuringPasswordChangeBubbleController::FinishPasswordChange() {
  if (password_change_delegate_) {
    password_change_delegate_->Stop();
  }
}

void OtpDuringPasswordChangeBubbleController::
    NavigateToPasswordChangeSettings() {
  delegate_->NavigateToPasswordChangeSettings();
}
