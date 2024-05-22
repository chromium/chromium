// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/save_unsynced_credentials_locally_bubble_controller.h"

#include <utility>

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "ui/base/l10n/l10n_util.h"

namespace metrics_util = password_manager::metrics_util;

SaveUnsyncedCredentialsLocallyBubbleController::
    SaveUnsyncedCredentialsLocallyBubbleController(
        base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          /*display_disposition=*/metrics_util::
              AUTOMATIC_SAVE_UNSYNCED_CREDENTIALS_LOCALLY),
      dismissal_reason_(metrics_util::NO_DIRECT_INTERACTION),
      unsynced_credentials_(delegate_->GetUnsyncedCredentials()) {}

SaveUnsyncedCredentialsLocallyBubbleController::
    ~SaveUnsyncedCredentialsLocallyBubbleController() {
  OnBubbleClosing();
}

void SaveUnsyncedCredentialsLocallyBubbleController::OnSaveClicked(
    const std::vector<bool>& was_credential_selected) {
  DCHECK(was_credential_selected.size() == unsynced_credentials_.size());
  std::vector<password_manager::PasswordForm> credentials_to_save;
  for (size_t i = 0; i < unsynced_credentials_.size(); i++) {
    if (was_credential_selected[i]) {
      credentials_to_save.push_back(unsynced_credentials_[i]);
    }
  }
  delegate_->SaveUnsyncedCredentialsInProfileStore(credentials_to_save);
}

void SaveUnsyncedCredentialsLocallyBubbleController::OnCancelClicked() {
  delegate_->DiscardUnsyncedCredentials();
}

void SaveUnsyncedCredentialsLocallyBubbleController::ReportInteractions() {
  metrics_util::LogGeneralUIDismissalReason(dismissal_reason_);
  // Record UKM statistics on dismissal reason.
  if (metrics_recorder_) {
    metrics_recorder_->RecordUIDismissalReason(dismissal_reason_);
  }
}

std::u16string SaveUnsyncedCredentialsLocallyBubbleController::GetTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UNSYNCED_CREDENTIALS_BUBBLE_TITLE_GPM);
}
