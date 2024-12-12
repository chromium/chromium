// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_PASSWORD_CHANGE_INFO_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_PASSWORD_CHANGE_INFO_BUBBLE_CONTROLLER_H_

#include <string>

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

// Controller for the views informing the user about the password change flow
// state.
class PasswordChangeInfoBubbleController : public PasswordBubbleControllerBase {
 public:
  explicit PasswordChangeInfoBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);

  ~PasswordChangeInfoBubbleController() override;

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_PASSWORD_CHANGE_INFO_BUBBLE_CONTROLLER_H_
