// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_STRATEGY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_STRATEGY_H_

namespace autofill::payments {

// Interface for objects that define a strategy for handling a BNPL autofill
// flow with different implementations meant to handle different operating
// systems. Created lazily in the PaymentsAutofillClient when it is needed.
class BnplStrategy {
 public:
  // Defines the next step that the BnplManager should take after the user has
  // been shown a payment method autofill suggestion. The strategy
  // implementation determines which action to return based on the platform.
  enum class SuggestionShownNextAction {
    // The flow should check if a BNPL suggestion is currently being shown.
    // If it isn't, then run the update suggestions barrier callback.
    kNotifyUpdateCallbackOfSuggestionsShownResponse = 0,

    // The flow does not need to run the update suggestions barrier callback.
    kSkipNotifyingUpdateCallbackOfSuggestionsShownResponse = 1,

    kMaxValue = kSkipNotifyingUpdateCallbackOfSuggestionsShownResponse,
  };

  // Defines the next step that the BnplManager should take after the user has
  // accepted a BNPL autofill suggestion. The strategy implementation determines
  // which action to return based on the platform.
  enum class BnplSuggestionAcceptedNextAction {
    // The flow should show the Select BNPL Issuer UI.
    kShowSelectBnplIssuerUi = 0,

    // The flow should check if amount extraction has finished extracting the
    // checkout amount from the webpage. If complete, show the BNPL selection
    // screen. Otherwise, show the progress screen.
    kCheckAmountExtractionBeforeContinuingFlow = 1,

    kMaxValue = kCheckAmountExtractionBeforeContinuingFlow,
  };

  // Defines the next step that the BnplManager should take after amount
  // extraction returns. The strategy implementation determines
  // which action to return based on the platform.
  enum class BnplAmountExtractionReturnedNextAction {
    // Run the update suggestions barrier callback.
    kNotifyUpdateCallbackOfAmountExtractionReturnedResponse = 0,

    // Notify the UI to update accordingly based on the amount extraction
    // response.
    kNotifyUiOfAmountExtractionReturnedResponse = 1,

    kMaxValue = kNotifyUiOfAmountExtractionReturnedResponse,
  };

  virtual ~BnplStrategy();

  // Returns the next action to take after the user has been shown a payment
  // method autofill suggestion.
  virtual SuggestionShownNextAction GetNextActionOnSuggestionShown();

  // Returns the next action to take after the user has accepted a BNPL
  // suggestion.
  virtual BnplSuggestionAcceptedNextAction
  GetNextActionOnBnplSuggestionAcceptance();

  // Returns the next action to take after the amount extraction is finished.
  virtual BnplAmountExtractionReturnedNextAction
  GetNextActionOnAmountExtractionReturned();
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_STRATEGY_H_
