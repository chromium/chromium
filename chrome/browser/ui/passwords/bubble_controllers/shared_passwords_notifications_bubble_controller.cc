// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/shared_passwords_notifications_bubble_controller.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/base/l10n/l10n_util.h"

using password_manager::PasswordForm;

SharedPasswordsNotificationBubbleController::
    SharedPasswordsNotificationBubbleController(
        base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          password_manager::metrics_util::
              AUTOMATIC_SHARED_PASSWORDS_NOTIFICATION) {}

SharedPasswordsNotificationBubbleController::
    ~SharedPasswordsNotificationBubbleController() {
  // Make sure the interactions are reported even if Views didn't notify the
  // controller about the bubble being closed.
  OnBubbleClosing();
}

std::u16string SharedPasswordsNotificationBubbleController::GetTitle() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_SHARED_PASSWORDS_NOTIFICATION_TITLE,
      GetSharedCredentialsRequiringNotification().size());
}

void SharedPasswordsNotificationBubbleController::ReportInteractions() {
  // TODO(crbug.com/1464209): Report necessary interactions.
}

std::vector<PasswordForm*> SharedPasswordsNotificationBubbleController::
    GetSharedCredentialsRequiringNotification() const {
  std::vector<PasswordForm*> shared_unnotified_credentials;
  for (const std::unique_ptr<PasswordForm>& form :
       delegate_->GetCurrentForms()) {
    if (form->type == PasswordForm::Type::kReceivedViaSharing &&
        !form->sharing_notification_displayed) {
      shared_unnotified_credentials.push_back(form.get());
    }
  }
  return shared_unnotified_credentials;
}
