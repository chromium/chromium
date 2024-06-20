// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/passkey_saved_confirmation_controller.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

PasskeySavedConfirmationController::PasskeySavedConfirmationController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          /*display_disposition=*/password_manager::metrics_util::
              AUTOMATIC_PASSKEY_SAVED_CONFIRMATION) {}

PasskeySavedConfirmationController::~PasskeySavedConfirmationController() {
  OnBubbleClosing();
}

std::u16string PasskeySavedConfirmationController::GetTitle() const {
  // TODO(b/345242100): Add string version with pin created.
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_PASSKEY_SAVED_TITLE);
}

void PasskeySavedConfirmationController::ReportInteractions() {
  password_manager::metrics_util::LogGeneralUIDismissalReason(
      password_manager::metrics_util::NO_DIRECT_INTERACTION);
}
