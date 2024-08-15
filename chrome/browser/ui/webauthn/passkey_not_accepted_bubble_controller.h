// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_NOT_ACCEPTED_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_NOT_ACCEPTED_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

// Manages the bubble which is shown as a notification when a passkey is
// not accepted.
class PasskeyNotAcceptedBubbleController : public PasswordBubbleControllerBase {
 public:
  PasskeyNotAcceptedBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate,
      password_manager::metrics_util::UIDisplayDisposition display_disposition);
  ~PasskeyNotAcceptedBubbleController() override;

  // PasswordBubbleControllerBase:
  std::u16string GetTitle() const override;

  // Called by the view when the user clicks the "Google Password Manager" link.
  // Navigates to password manager main page.
  void OnGooglePasswordManagerLinkClicked();

 private:
  // PasswordBubbleControllerBase:
  void ReportInteractions() override;

  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_NOT_ACCEPTED_BUBBLE_CONTROLLER_H_
