// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FAST_CHECKOUT_FAST_CHECKOUT_CONTROLLER_H_
#define CHROME_BROWSER_UI_FAST_CHECKOUT_FAST_CHECKOUT_CONTROLLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/native_widget_types.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

// Abstract interface for a Fast Checkout controller.
class FastCheckoutController {
 public:
  virtual ~FastCheckoutController() = default;

  // Instructs the controller to show the stored autofill profiles and
  // credit cards to the user.
  virtual void Show(
      const std::vector<const autofill::AutofillProfile*>& autofill_profiles,
      const std::vector<autofill::CreditCard*>& credit_cards) = 0;

  // Informs the controller that the user has made a selection.
  virtual void OnOptionsSelected(
      std::unique_ptr<autofill::AutofillProfile> profile,
      std::unique_ptr<autofill::CreditCard> credit_card) = 0;

  // Informs the controller that the user has dismissed the sheet.
  virtual void OnDismiss() = 0;

  // Opens the settings menu for Autofill profiles.
  virtual void OpenAutofillProfileSettings() = 0;

  // Opens the settings menu for credit cards.
  virtual void OpenCreditCardSettings() = 0;

  // The web page view containing the focused field.
  virtual gfx::NativeView GetNativeView() = 0;
};

#endif  // CHROME_BROWSER_UI_FAST_CHECKOUT_FAST_CHECKOUT_CONTROLLER_H_
