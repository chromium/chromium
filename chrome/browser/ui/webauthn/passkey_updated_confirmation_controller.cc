// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/passkey_updated_confirmation_controller.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/base/l10n/l10n_util.h"

PasskeyUpdatedConfirmationController::PasskeyUpdatedConfirmationController(
    base::WeakPtr<PasswordsModelDelegate> delegate,
    password_manager::metrics_util::UIDisplayDisposition display_disposition)
    : PasswordBubbleControllerBase(std::move(delegate), display_disposition) {}

PasskeyUpdatedConfirmationController::~PasskeyUpdatedConfirmationController() {
  OnBubbleClosing();
}

std::u16string PasskeyUpdatedConfirmationController::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_PASSKEY_UPDATED_TITLE);
}

void PasskeyUpdatedConfirmationController::
    OnGooglePasswordManagerLinkClicked() {
  dismissal_reason_ = password_manager::metrics_util::CLICKED_MANAGE;
  if (delegate_) {
    delegate_->NavigateToPasswordManagerSettingsPage(
        password_manager::ManagePasswordsReferrer::
            kPasskeyUpdatedConfirmationBubble);
  }
}

void PasskeyUpdatedConfirmationController::ReportInteractions() {
  password_manager::metrics_util::LogGeneralUIDismissalReason(
      dismissal_reason_);
}
