// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_PASSWORD_CHANGE_INFO_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_PASSWORD_CHANGE_INFO_BUBBLE_CONTROLLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

// Controller for the views informing the user about the password change flow
// state.
class PasswordChangeInfoBubbleController : public PasswordBubbleControllerBase,
                                           PasswordChangeDelegate::Observer {
 public:
  explicit PasswordChangeInfoBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate,
      PasswordChangeDelegate::State state);

  ~PasswordChangeInfoBubbleController() override;

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  // PasswordChangeDelegate::Observer methods:
  void OnStateChanged(PasswordChangeDelegate::State new_state) override;
  void OnPasswordChangeStopped(PasswordChangeDelegate* delegate) override;

  void CancelPasswordChange();
  // Get the change password origin to be displayed in UI.
  std::u16string GetDisplayOrigin();
  void OnGooglePasswordManagerLinkClicked();
  std::u16string GetPrimaryAccountEmail();

 private:
  PasswordChangeDelegate::State state_;
  base::WeakPtr<PasswordChangeDelegate> password_change_delegate_;
  // Dismissal reason for this bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_PASSWORD_CHANGE_INFO_BUBBLE_CONTROLLER_H_
