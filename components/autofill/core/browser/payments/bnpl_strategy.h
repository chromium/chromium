// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_STRATEGY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_STRATEGY_H_

#include "components/autofill/core/browser/payments/payments_autofill_client.h"

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

  // Defines the step that the BnplManager should take before switching to the
  // next view, based on the platform.
  enum class BeforeSwitchingViewAction {
    // The UI code should handle the switching. BnplManager will
    // continue to show the next view directly.
    kDoNothing = 0,

    // Close the current view before showing the next one.
    kCloseCurrentUi = 1,

    kMaxValue = kCloseCurrentUi,
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

  // Returns the action to take before switching to the next view.
  virtual BeforeSwitchingViewAction GetBeforeViewSwitchAction();

  // Returns whether the existing UI should be removed after a server response.
  // `result` is used by platforms to check if the UI should remain open.
  virtual bool ShouldRemoveExistingUiOnServerReturn(
      PaymentsAutofillClient::PaymentsRpcResult result) = 0;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_BNPL_STRATEGY_H_
