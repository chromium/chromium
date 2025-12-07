// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/desktop_bnpl_strategy.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class DesktopBnplStrategyTest : public testing::Test {
 public:
  DesktopBnplStrategyTest() = default;
  ~DesktopBnplStrategyTest() override = default;

 protected:
  DesktopBnplStrategy desktop_bnpl_strategy_;
};

// Verify that GetNextActionOnSuggestionShown() returns the correct action for
// the desktop platform.
TEST_F(DesktopBnplStrategyTest, GetNextActionOnSuggestionShown) {
  EXPECT_EQ(desktop_bnpl_strategy_.GetNextActionOnSuggestionShown(),
            BnplStrategy::SuggestionShownNextAction::
                kNotifyUpdateCallbackOfSuggestionsShownResponse);
}

// Verify that GetNextActionOnBnplSuggestionAcceptance() returns the correct
// action for the desktop platform.
TEST_F(DesktopBnplStrategyTest, GetNextActionOnBnplSuggestionAcceptance) {
  EXPECT_EQ(desktop_bnpl_strategy_.GetNextActionOnBnplSuggestionAcceptance(),
            BnplStrategy::BnplSuggestionAcceptedNextAction::
                kShowSelectBnplIssuerUi);
}

// Verify that GetNextActionOnAmountExtractionReturned() returns the correct
// action for the desktop platform.
TEST_F(DesktopBnplStrategyTest, GetNextActionOnAmountExtractionReturned) {
  EXPECT_EQ(desktop_bnpl_strategy_.GetNextActionOnAmountExtractionReturned(),
            BnplStrategy::BnplAmountExtractionReturnedNextAction::
                kNotifyUpdateCallbackOfAmountExtractionReturnedResponse);
}

// Verify that GetBeforeViewSwitchAction() returns the correct action for
// the desktop platform.
TEST_F(DesktopBnplStrategyTest, GetBeforeViewSwitchAction) {
  EXPECT_EQ(desktop_bnpl_strategy_.GetBeforeViewSwitchAction(),
            BnplStrategy::BeforeSwitchingViewAction::kCloseCurrentUi);
}

// Verify that ShouldRemoveExistingUiOnServerReturn() returns the correct
// value for the desktop platform.
TEST_F(DesktopBnplStrategyTest, ShouldRemoveExistingUiOnServerReturn) {
  EXPECT_EQ(desktop_bnpl_strategy_.ShouldRemoveExistingUiOnServerReturn(
                PaymentsAutofillClient::PaymentsRpcResult::kSuccess),
            true);
  EXPECT_EQ(desktop_bnpl_strategy_.ShouldRemoveExistingUiOnServerReturn(
                PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure),
            true);
}

}  // namespace autofill::payments
