// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_credential_leak_bubble_controller.h"

#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

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

void PasswordChangeCredentialLeakBubbleController::ChangePassword() {
  password_change_delegate_->StartPasswordChangeFlow();
}
