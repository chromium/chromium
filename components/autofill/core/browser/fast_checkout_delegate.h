// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FAST_CHECKOUT_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FAST_CHECKOUT_DELEGATE_H_

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// Delegate for in-browser Fast Checkout (FC) surface display and selection.
// Currently FC surface is eligible only for particular forms on click on
// an empty focusable text input field.
//
// It is supposed to be owned by the given `BrowserAutofillManager`, and
// interact with it and its `AutofillClient` and `AutofillDriver`.
class FastCheckoutDelegate {
 public:
  virtual ~FastCheckoutDelegate() = default;

  // Checks whether FastCheckout is eligible for the given web form data. On
  // success triggers the corresponding surface and returns `true`.
  virtual bool TryToShowFastCheckout(const FormFieldData& field) = 0;

  // Returns whether the FC surface is currently being shown.
  virtual bool IsShowingFastCheckoutUI() const = 0;

  // Hides the FC surface if one is shown.
  virtual void HideFastCheckoutUI() = 0;

  // Triggered after the fast checkout card is closed, either by dissmisal or by
  // accepting the options.
  virtual void OnFastCheckoutUIHidden() = 0;

  // Resets the internal state of the delegate.
  virtual void Reset() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FAST_CHECKOUT_DELEGATE_H_
