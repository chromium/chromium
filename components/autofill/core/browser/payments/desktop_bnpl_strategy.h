// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_DESKTOP_BNPL_STRATEGY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_DESKTOP_BNPL_STRATEGY_H_

#include "components/autofill/core/browser/payments/bnpl_strategy.h"

namespace autofill::payments {

// Desktop implementation of the `BnplStrategy` interface. This class defines
// the logic for a BNPL flow on the Desktop platform.
class DesktopBnplStrategy : public BnplStrategy {
 public:
  DesktopBnplStrategy();
  DesktopBnplStrategy(const DesktopBnplStrategy&) = delete;
  DesktopBnplStrategy& operator=(const DesktopBnplStrategy&) = delete;
  ~DesktopBnplStrategy() override;

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

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_DESKTOP_BNPL_STRATEGY_H_
