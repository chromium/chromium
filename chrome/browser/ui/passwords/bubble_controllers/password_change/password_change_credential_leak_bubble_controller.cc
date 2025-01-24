// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_credential_leak_bubble_controller.h"

#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

PasswordChangeCredentialLeakBubbleController::
    PasswordChangeCredentialLeakBubbleController(
        base::WeakPtr<PasswordsModelDelegate> delegate,
        base::WeakPtr<PasswordsLeakDialogDelegate> leak_dialog_delegate,
        password_manager::LeakedPasswordDetails details)
    : PasswordBubbleControllerBase(
          delegate,
          password_manager::metrics_util::UIDisplayDisposition::
              PASSWORD_CHANGE_BUBBLE),
      leak_dialog_delegate_(leak_dialog_delegate),
      username_(std::move(details.username)),
      password_(std::move(details.password)),
      url_(std::move(details.origin)) {}

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
  leak_dialog_delegate_->ChangePassword(url_, username_, password_);
}
