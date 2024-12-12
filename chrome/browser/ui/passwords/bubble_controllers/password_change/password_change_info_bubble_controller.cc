// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_info_bubble_controller.h"

PasswordChangeInfoBubbleController::PasswordChangeInfoBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          delegate,
          password_manager::metrics_util::UIDisplayDisposition::
              PASSWORD_CHANGE_BUBBLE) {}

PasswordChangeInfoBubbleController::~PasswordChangeInfoBubbleController() {
  OnBubbleClosing();
}

std::u16string PasswordChangeInfoBubbleController::GetTitle() const {
  // TODO(crbug.com/381053884): Return correct title (depends on
  // PasswordChangeDelegate::State).
  return u"";
}

void PasswordChangeInfoBubbleController::ReportInteractions() {
  // TODO(crbug.com/381053884): Report metrics.
}
