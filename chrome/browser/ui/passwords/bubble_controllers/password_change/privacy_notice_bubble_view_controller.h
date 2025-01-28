// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_PRIVACY_NOTICE_BUBBLE_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_PRIVACY_NOTICE_BUBBLE_VIEW_CONTROLLER_H_

#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

// Controller for the privacy notice view. The privacy notice needs to be
// accepted to start the password change flow.
class PrivacyNoticeBubbleViewController : public PasswordBubbleControllerBase {
 public:
  explicit PrivacyNoticeBubbleViewController(
      base::WeakPtr<PasswordsModelDelegate> delegate);

  ~PrivacyNoticeBubbleViewController() override;

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  void AcceptNotice();

  // Cancels the flow.
  void Cancel();

 private:
  base::WeakPtr<PasswordChangeDelegate> password_change_delegate_;
  // Dismissal reason for this bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_PRIVACY_NOTICE_BUBBLE_VIEW_CONTROLLER_H_
