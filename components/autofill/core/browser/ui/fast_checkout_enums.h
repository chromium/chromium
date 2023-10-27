// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_FAST_CHECKOUT_ENUMS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_FAST_CHECKOUT_ENUMS_H_

namespace autofill {
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
  // The sheet was not shown because AutofillProfile setting is disabled.
  kFailureAutofillProfileDisabled = 7,
  // The sheet was not shown because AutofillCreditCard setting is disabled.
  kFailureAutofillCreditCardDisabled = 8,
  // FastCheckout is not supported for this field type. This value is not logged
  // to UMA.
  kUnsupportedFieldType = 9,
  // FastCheckout is not supported for this country.
  kUnsupportedCountry = 10,

  kMaxValue = kUnsupportedCountry
};

// Enum defining possible outcomes of a Fast Checkout run.
enum class FastCheckoutRunOutcome {
  // Run was successful, i.e. all forms were filled.
  kSuccess = 0,
  // The bottomsheet was dismissed. No forms were filled.
  kBottomsheetDismissed = 1,
  // Run timed out before all forms could have been filled.
  kTimeout = 2,
  // Run ended because the tab was closed before all forms were filled.
  kTabClosed = 3,
  // Origin-changing navigation occurred before all forms were filled.
  kOriginChange = 4,
  // Navigation to a non-checkout page occurred before all forms were filled.
  kNonCheckoutPage = 5,
  // Navigation occurred while bottomsheet was shown.
  kNavigationWhileBottomsheetWasShown = 6,
  // Personal data has become invalid while bottomsheet was shown.
  kInvalidPersonalData = 7,
  // Autofill manager got destroyed.
  kAutofillManagerDestroyed = 8,
  // The CVC popup was closed before all forms were filled.
  kCvcPopupClosed = 9,
  // An error occurred while requesting the full card before all forms were
  // filled.
  kCvcPopupError = 10,
  // Autofill profile was deleted since its selection in the UI.
  kAutofillProfileDeleted = 11,
  // Credit card was deleted since its selection in the UI.
  kCreditCardDeleted = 12,
  // Page was refreshed during a run.
  kPageRefreshed = 13,
  kMaxValue = kPageRefreshed
};

// Represents the state of the bottomsheet.
enum class FastCheckoutUIState {
  kNotShownYet,
  kIsShowing,
  kWasShown,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_FAST_CHECKOUT_ENUMS_H_
