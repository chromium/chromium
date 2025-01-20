// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_INTERFACE_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_INTERFACE_H_

#include <string>

#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "components/password_manager/core/browser/password_cross_domain_confirmation_popup_controller.h"

class PasswordCrossDomainConfirmationPopupControllerInterface
    : public password_manager::PasswordCrossDomainConfirmationPopupController,
      public autofill::AutofillPopupViewDelegate {
 public:
  // Returns the localized body text of the popup.
  virtual std::u16string GetBodyText() const = 0;

  // Returns the localized title text of the popup.
  virtual std::u16string GetTitleText() const = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CROSS_DOMAIN_CONFIRMATION_POPUP_CONTROLLER_INTERFACE_H_
