// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_BIOMETRIC_AUTHENTICATION_CONFIRMATION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_BIOMETRIC_AUTHENTICATION_CONFIRMATION_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

class PasswordsModelDelegate;

// This controller manages the bubble which is shown after enabling biometric
// authentication before filling passwords.
class BiometricAuthenticationConfirmationBubbleController
    : public PasswordBubbleControllerBase {
 public:
  explicit BiometricAuthenticationConfirmationBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~BiometricAuthenticationConfirmationBubbleController() override;

  std::u16string GetTitle() const override;
  int GetImageID(bool dark) const;
  void OnNavigateToSettingsLinkClicked();

 private:
  // PasswordBubbleControllerBase:
  void ReportInteractions() override {}
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_BIOMETRIC_AUTHENTICATION_CONFIRMATION_BUBBLE_CONTROLLER_H_
