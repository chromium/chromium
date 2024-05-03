// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/add_username_bubble_controller.h"

#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "ui/base/l10n/l10n_util.h"

namespace metrics_util = password_manager::metrics_util;

AddUsernameBubbleController::AddUsernameBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate,
    DisplayReason display_reason)
    : CommonSavedAccountManagerBubbleController(
          delegate,
          display_reason,
          display_reason ==
                  PasswordBubbleControllerBase::DisplayReason::kUserAction
              ? metrics_util::MANUAL_ADD_USERNAME_BUBBLE
              : metrics_util::AUTOMATIC_ADD_USERNAME_BUBBLE),
      ininial_pending_password_(pending_password()) {}

AddUsernameBubbleController::~AddUsernameBubbleController() {
  OnBubbleClosing();
}

void AddUsernameBubbleController::OnSaveClicked() {
  SetDismissalReason(metrics_util::CLICKED_ACCEPT);
  if (!delegate_) {
    return;
  }
  delegate_->OnAddUsernameSaveClicked(pending_password().username_value,
                                      ininial_pending_password_);
}

std::u16string AddUsernameBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_ADD_USERNAME_TITLE);
}

void AddUsernameBubbleController::ReportInteractions() {
  password_manager::metrics_util::LogGeneralUIDismissalReason(
      GetDismissalReason());
  base::UmaHistogramBoolean(
      "PasswordBubble.AddUsernameBubble.UsernameAdded",
      GetDismissalReason() == metrics_util::CLICKED_ACCEPT);
}
