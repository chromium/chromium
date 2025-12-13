// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_strategy.h"

#include "base/notreached.h"

namespace autofill::payments {

BnplStrategy::~BnplStrategy() = default;

BnplStrategy::SuggestionShownNextAction
BnplStrategy::GetNextActionOnSuggestionShown() {
  NOTREACHED();
}

BnplStrategy::BnplSuggestionAcceptedNextAction
BnplStrategy::GetNextActionOnBnplSuggestionAcceptance() {
  NOTREACHED();
}

BnplStrategy::BnplAmountExtractionReturnedNextAction
BnplStrategy::GetNextActionOnAmountExtractionReturned() {
  NOTREACHED();
}

BnplStrategy::BeforeSwitchingViewAction
BnplStrategy::GetBeforeViewSwitchAction() {
  NOTREACHED();
}

bool BnplStrategy::ShouldRemoveExistingUiOnServerReturn(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  NOTREACHED();
}

}  // namespace autofill::payments
