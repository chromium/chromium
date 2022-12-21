// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FAST_CHECKOUT_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FAST_CHECKOUT_DELEGATE_H_

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

class AutofillDriver;

// Enum that describes different outcomes to an attempt of triggering the
// FastCheckout bottomsheet.
// Do not remove or renumber entries in this enum. It needs to be kept in
// sync with the enum of the same name in `enums.xml`.
// The enum values are not exhaustive to avoid excessive metric collection.
// Instead focus on the most interesting abort cases and only deal with cases
// in which the FastCheckout feature is enabled and a script exists for the
// form in question.
enum class FastCheckoutTriggerOutcome {
  // The sheet was shown.
  kSuccess = 0,
  // The sheet was not shown because it has already been shown before.
  kFailureShownBefore = 1,
  // The sheet was not shown because the clicked field is not focusable.
  kFailureFieldNotFocusable = 2,
  // The sheet was not shown because the clicked field is not empty.
  kFailureFieldNotEmpty = 3,
  // The sheet was not shown because Autofill UI cannot be shown.
  kFailureCannotShowAutofillUi = 4,
  // The sheet was not shown because there is no valid credit card.
  kFailureNoValidCreditCard = 5,
  // The sheet was not shown because there is no valid Autofill profile.
  kFailureNoValidAutofillProfile = 6,
  kMaxValue = kFailureNoValidAutofillProfile
};

constexpr char kUmaKeyFastCheckoutTriggerOutcome[] =
    "Autofill.FastCheckout.TriggerOutcome";

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
  virtual bool TryToShowFastCheckout(const FormData& form,
                                     const FormFieldData& field) = 0;

  // Returns whether the FC surface is currently being shown.
  virtual bool IsShowingFastCheckoutUI() const = 0;

  // Hides the FC surface if one is shown.
  virtual void HideFastCheckoutUI() = 0;

  // Triggered after the fast checkout card is closed, either by dissmisal or by
  // accepting the options.
  virtual void OnFastCheckoutUIHidden() = 0;

  // Returns a raw pointer to the Autofill driver. On implementations other than
  // iOS, the pointer can safely be cast to a `ContentAutofillDriver*`.
  virtual AutofillDriver* GetDriver() = 0;

  // Resets the internal state of the delegate.
  virtual void Reset() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FAST_CHECKOUT_DELEGATE_H_
