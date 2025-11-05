// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/android_bnpl_strategy.h"

namespace autofill::payments {

AndroidBnplStrategy::AndroidBnplStrategy() = default;

AndroidBnplStrategy::~AndroidBnplStrategy() = default;

BnplStrategy::SuggestionShownNextAction
AndroidBnplStrategy::GetNextActionOnSuggestionShown() {
  return SuggestionShownNextAction::
      kSkipNotifyingUpdateCallbackOfSuggestionsShownResponse;
}

BnplStrategy::BnplSuggestionAcceptedNextAction
AndroidBnplStrategy::GetNextActionOnBnplSuggestionAcceptance() {
  return BnplSuggestionAcceptedNextAction::
      kCheckAmountExtractionBeforeContinuingFlow;
}

BnplStrategy::BnplAmountExtractionReturnedNextAction
AndroidBnplStrategy::GetNextActionOnAmountExtractionReturned() {
  return BnplAmountExtractionReturnedNextAction::
      kNotifyUiOfAmountExtractionReturnedResponse;
}

BnplStrategy::BeforeSwitchingViewAction
AndroidBnplStrategy::GetBeforeViewSwitchAction() {
  // With `ViewFlipper` on Android, the current screen is flipped to the next
  // screen within the same view, so no need to close the current screen
  // before opening the next screen.
  return BeforeSwitchingViewAction::kDoNothing;
}

bool AndroidBnplStrategy::ShouldRemoveExistingUiOnServerReturn(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  return result == PaymentsAutofillClient::PaymentsRpcResult::kSuccess;
}

}  // namespace autofill::payments
