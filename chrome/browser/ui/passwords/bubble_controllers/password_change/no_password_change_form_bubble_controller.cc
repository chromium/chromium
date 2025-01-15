// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/no_password_change_form_bubble_controller.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

NoPasswordChangeFormBubbleController::NoPasswordChangeFormBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          delegate,
          password_manager::metrics_util::UIDisplayDisposition::
              PASSWORD_CHANGE_BUBBLE),
      password_change_delegate_(
          delegate_->GetPasswordChangeDelegate()->AsWeakPtr()) {}

NoPasswordChangeFormBubbleController::~NoPasswordChangeFormBubbleController() {
  OnBubbleClosing();
}

void NoPasswordChangeFormBubbleController::Restart() {
  // TODO(crbug.com/381055148): Implement logic.
}

void NoPasswordChangeFormBubbleController::Cancel() {
  CHECK(password_change_delegate_);
  password_change_delegate_->Stop();
}

std::u16string NoPasswordChangeFormBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_RETRY_TITLE);
}

std::u16string NoPasswordChangeFormBubbleController::GetBody() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_RETRY_BODY);
}

std::u16string NoPasswordChangeFormBubbleController::GetAcceptButton() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_RETRY_ACTION);
}

void NoPasswordChangeFormBubbleController::ReportInteractions() {
  // TODO(crbug.com/381055148): Report metrics.
}
