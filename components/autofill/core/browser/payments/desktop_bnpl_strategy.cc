// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/desktop_bnpl_strategy.h"

namespace autofill::payments {

DesktopBnplStrategy::DesktopBnplStrategy() = default;

DesktopBnplStrategy::~DesktopBnplStrategy() = default;

BnplStrategy::SuggestionShownNextAction
DesktopBnplStrategy::GetNextActionOnSuggestionShown() {
  return SuggestionShownNextAction::
      kNotifyUpdateCallbackOfSuggestionsShownResponse;
}

BnplStrategy::BnplSuggestionAcceptedNextAction
DesktopBnplStrategy::GetNextActionOnBnplSuggestionAcceptance() {
  return BnplSuggestionAcceptedNextAction::kShowSelectBnplIssuerUi;
}

BnplStrategy::BnplAmountExtractionReturnedNextAction
DesktopBnplStrategy::GetNextActionOnAmountExtractionReturned() {
  return BnplAmountExtractionReturnedNextAction::
      kNotifyUpdateCallbackOfAmountExtractionReturnedResponse;
}

BnplStrategy::BeforeSwitchingViewAction
DesktopBnplStrategy::GetBeforeViewSwitchAction() {
  return BeforeSwitchingViewAction::kCloseCurrentUi;
}

bool DesktopBnplStrategy::ShouldRemoveExistingUiOnServerReturn(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  return true;
}

}  // namespace autofill::payments
