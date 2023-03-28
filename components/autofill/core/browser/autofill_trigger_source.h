// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TRIGGER_SOURCE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TRIGGER_SOURCE_H_

namespace autofill {

// Specifies the source that triggered autofilling a form.
enum class AutofillTriggerSource {
  // Autofill was triggered from accepting a suggestion in the Autofill popup.
  kPopup = 0,
  // Autofill was triggered from accepting a suggestion in the keyboard
  // accessory.
  kKeyboardAccessory = 1,
  // Autofill was triggered from accepting a suggestion in the touch to fill for
  // credit cards bottom sheet.
  kTouchToFillCreditCard = 2,
  // Refill was triggered from the forms seen event. This includes cases where a
  // refill was triggered right after a non-refill Autofill invocation - in this
  // case the original trigger source got lost.
  kFormsSeen = 3,
  // Refill was triggered from blink when the selected option of a <select>
  // control is changed.
  kSelectOptionsChanged = 4,
  // Refill was triggered from blink when the input element is in the autofilled
  // state and the value has been changed by JavaScript.
  kJavaScriptChangedAutofilledValue = 5,
  // Autofill was applied after unlocking a server card with the CVC. The
  // original trigger source got lost. This should not happen.
  kCreditCardCvcPopup = 6,
  // Autofill was triggered from a Fast Checkout run.
  kFastCheckout = 7
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TRIGGER_SOURCE_H_
