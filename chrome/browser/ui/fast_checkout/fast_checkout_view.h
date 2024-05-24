// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FAST_CHECKOUT_FAST_CHECKOUT_VIEW_H_
#define CHROME_BROWSER_UI_FAST_CHECKOUT_FAST_CHECKOUT_VIEW_H_

#include "base/containers/span.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

class FastCheckoutView {
 public:
  FastCheckoutView() = default;
  FastCheckoutView(const FastCheckoutView&) = delete;
  FastCheckoutView& operator=(const FastCheckoutView&) = delete;
  virtual ~FastCheckoutView() = default;

  // Show the sheet with provided options to the user. After user interaction,
  // either `OnCredentialSelected` or `OnDismiss` gets invoked.
  virtual void Show(
      const std::vector<const autofill::AutofillProfile*>& autofill_profiles,
      const std::vector<autofill::CreditCard*>& credit_cards) = 0;

  // Factory function for creating the view.
  static std::unique_ptr<FastCheckoutView> Create(
      base::WeakPtr<FastCheckoutController> controller);
};

#endif  // CHROME_BROWSER_UI_FAST_CHECKOUT_FAST_CHECKOUT_VIEW_H_
