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

}  // namespace autofill::payments
