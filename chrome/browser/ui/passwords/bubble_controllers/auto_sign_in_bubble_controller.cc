// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/auto_sign_in_bubble_controller.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"

namespace metrics_util = password_manager::metrics_util;

AutoSignInBubbleController::AutoSignInBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(std::move(delegate),
                                   /*display_disposition=*/password_manager::
                                       metrics_util::AUTOMATIC_SIGNIN_TOAST),
      dismissal_reason_(metrics_util::NO_DIRECT_INTERACTION) {
  pending_password_ = delegate_->GetPendingPassword();
}

AutoSignInBubbleController::~AutoSignInBubbleController() {
  OnBubbleClosing();
}

void AutoSignInBubbleController::OnAutoSignInToastTimeout() {
  dismissal_reason_ = metrics_util::AUTO_SIGNIN_TOAST_TIMEOUT;
}

std::u16string AutoSignInBubbleController::GetTitle() const {
  return std::u16string();
}

void AutoSignInBubbleController::ReportInteractions() {
  metrics_util::LogGeneralUIDismissalReason(dismissal_reason_);
  // Record UKM statistics on dismissal reason.
  if (metrics_recorder_) {
    metrics_recorder_->RecordUIDismissalReason(dismissal_reason_);
  }
}
