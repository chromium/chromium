// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_SAVED_CONFIRMATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_SAVED_CONFIRMATION_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

// Manages the bubble which is shown as a confirmation when a passkey is saved.
class PasskeySavedConfirmationController : public PasswordBubbleControllerBase {
 public:
  explicit PasskeySavedConfirmationController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~PasskeySavedConfirmationController() override;

  // PasswordBubbleControllerBase:
  std::u16string GetTitle() const override;

 private:
  // PasswordBubbleControllerBase:
  void ReportInteractions() override;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_SAVED_CONFIRMATION_CONTROLLER_H_
