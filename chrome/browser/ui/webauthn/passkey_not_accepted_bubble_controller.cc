// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/passkey_not_accepted_bubble_controller.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/base/l10n/l10n_util.h"

PasskeyNotAcceptedBubbleController::PasskeyNotAcceptedBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate,
    password_manager::metrics_util::UIDisplayDisposition display_disposition)
    : PasswordBubbleControllerBase(std::move(delegate), display_disposition) {}

PasskeyNotAcceptedBubbleController::~PasskeyNotAcceptedBubbleController() {
  OnBubbleClosing();
}

std::u16string PasskeyNotAcceptedBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_PASSKEY_DELETED_TITLE);
}

void PasskeyNotAcceptedBubbleController::OnGooglePasswordManagerLinkClicked() {
  dismissal_reason_ = password_manager::metrics_util::CLICKED_MANAGE;
  if (delegate_) {
    delegate_->NavigateToPasswordManagerSettingsPage(
        password_manager::ManagePasswordsReferrer::kPasskeyNotAcceptedBubble);
  }
}

void PasskeyNotAcceptedBubbleController::ReportInteractions() {
  password_manager::metrics_util::LogGeneralUIDismissalReason(
      dismissal_reason_);
}
