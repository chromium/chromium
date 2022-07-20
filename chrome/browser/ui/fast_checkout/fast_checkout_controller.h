// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FAST_CHECKOUT_FAST_CHECKOUT_CONTROLLER_H_
#define CHROME_BROWSER_UI_FAST_CHECKOUT_FAST_CHECKOUT_CONTROLLER_H_

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
  virtual void Show() = 0;

  // Informs the controller that the user has made a selection.
  virtual void OnOptionsSelected(
      std::unique_ptr<autofill::AutofillProfile> profile,
      std::unique_ptr<autofill::CreditCard> credit_card) = 0;

  // Informs the controller that the user has dismissed the sheet.
  virtual void OnDismiss() = 0;

  // The web page view containing the focused field.
  virtual gfx::NativeView GetNativeView() = 0;
};

#endif  // CHROME_BROWSER_UI_FAST_CHECKOUT_FAST_CHECKOUT_CONTROLLER_H_
