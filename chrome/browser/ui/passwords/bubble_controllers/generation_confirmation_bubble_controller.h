// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_GENERATION_CONFIRMATION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_GENERATION_CONFIRMATION_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "ui/gfx/range/range.h"

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

  const std::u16string& save_confirmation_text() const {
    return save_confirmation_text_;
  }
  const gfx::Range& save_confirmation_link_range() const {
    return save_confirmation_link_range_;
  }

 private:
  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  std::u16string save_confirmation_text_;
  gfx::Range save_confirmation_link_range_;
  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_GENERATION_CONFIRMATION_BUBBLE_CONTROLLER_H_
