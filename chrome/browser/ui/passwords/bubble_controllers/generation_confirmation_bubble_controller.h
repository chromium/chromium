// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_GENERATION_CONFIRMATION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_GENERATION_CONFIRMATION_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"

// This controller provides data and actions for the
// PasswordGenerationConfirmationView.
class GenerationConfirmationBubbleController
    : public PasswordBubbleControllerBase {
 public:
  GenerationConfirmationBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate,
      DisplayReason display_reason);
  ~GenerationConfirmationBubbleController() override;

  // Called by the view code when the navigate to passwords.google.com link is
  // clicked by the user.
  void OnNavigateToPasswordManagerAccountDashboardLinkClicked(
      password_manager::ManagePasswordsReferrer referrer);

 private:
  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_GENERATION_CONFIRMATION_BUBBLE_CONTROLLER_H_
