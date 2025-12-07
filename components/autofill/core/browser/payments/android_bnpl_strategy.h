// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_ANDROID_BNPL_STRATEGY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_ANDROID_BNPL_STRATEGY_H_

#include "components/autofill/core/browser/payments/bnpl_strategy.h"

namespace autofill::payments {

// Android implementation of the `BnplStrategy` interface. This class defines
// the logic for a BNPL flow on the Android platform.
class AndroidBnplStrategy : public BnplStrategy {
 public:
  AndroidBnplStrategy();
  AndroidBnplStrategy(const AndroidBnplStrategy&) = delete;
  AndroidBnplStrategy& operator=(const AndroidBnplStrategy&) = delete;
  ~AndroidBnplStrategy() override;

  // BnplStrategy:
  SuggestionShownNextAction GetNextActionOnSuggestionShown() override;
  BnplSuggestionAcceptedNextAction GetNextActionOnBnplSuggestionAcceptance()
      override;
  BnplAmountExtractionReturnedNextAction
  GetNextActionOnAmountExtractionReturned() override;
  BeforeSwitchingViewAction GetBeforeViewSwitchAction() override;
  bool ShouldRemoveExistingUiOnServerReturn(
      PaymentsAutofillClient::PaymentsRpcResult result) override;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_ANDROID_BNPL_STRATEGY_H_
