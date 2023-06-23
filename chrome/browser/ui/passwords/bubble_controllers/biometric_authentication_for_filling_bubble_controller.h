// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_BIOMETRIC_AUTHENTICATION_FOR_FILLING_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_BIOMETRIC_AUTHENTICATION_FOR_FILLING_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "components/prefs/pref_service.h"

class PasswordsModelDelegate;

// This controller manages the bubble suggestng enabling biometric
// authentication before filling passwords.
class BiometricAuthenticationForFillingBubbleController
    : public PasswordBubbleControllerBase {
 public:
  explicit BiometricAuthenticationForFillingBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate,
      PrefService* prefs,
      PasswordBubbleControllerBase::DisplayReason display_reason);
  ~BiometricAuthenticationForFillingBubbleController() override;

  std::u16string GetTitle() const override;

  std::u16string GetBody() const;
  std::u16string GetContinueButtonText() const;
  std::u16string GetNoThanksButtonText() const;
  int GetImageID(bool dark) const;

  // The user chose to enable biometric authenticaion before filling passwords.
  void OnAccepted();
  // The user chose not to enable biometric authenticaion before filling
  // passwords.
  void OnCanceled();

 private:
  // PasswordBubbleControllerBase:
  void ReportInteractions() override;

  bool accept_clicked_ = false;

  raw_ptr<PrefService, DanglingUntriaged> prefs_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_BIOMETRIC_AUTHENTICATION_FOR_FILLING_BUBBLE_CONTROLLER_H_
