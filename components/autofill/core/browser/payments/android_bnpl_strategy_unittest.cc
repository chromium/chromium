// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/android_bnpl_strategy.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class AndroidBnplStrategyTest : public testing::Test {
 public:
  AndroidBnplStrategyTest() = default;
  ~AndroidBnplStrategyTest() override = default;

 protected:
  AndroidBnplStrategy android_bnpl_strategy_;
};

// Verify that GetNextActionOnSuggestionShown() returns the correct action for
// the Android platform.
TEST_F(AndroidBnplStrategyTest, GetNextActionOnSuggestionShown) {
  EXPECT_EQ(android_bnpl_strategy_.GetNextActionOnSuggestionShown(),
            BnplStrategy::SuggestionShownNextAction::
                kSkipNotifyingUpdateCallbackOfSuggestionsShownResponse);
}

// Verify that GetNextActionOnBnplSuggestionAcceptance() returns the correct
// action for the Android platform.
TEST_F(AndroidBnplStrategyTest, GetNextActionOnBnplSuggestionAcceptance) {
  EXPECT_EQ(android_bnpl_strategy_.GetNextActionOnBnplSuggestionAcceptance(),
            BnplStrategy::BnplSuggestionAcceptedNextAction::
                kCheckAmountExtractionBeforeContinuingFlow);
}

// Verify that GetNextActionOnAmountExtractionReturned() returns the correct
// action for the Android platform.
TEST_F(AndroidBnplStrategyTest, GetNextActionOnAmountExtractionReturned) {
  EXPECT_EQ(android_bnpl_strategy_.GetNextActionOnAmountExtractionReturned(),
            BnplStrategy::BnplAmountExtractionReturnedNextAction::
                kNotifyUiOfAmountExtractionReturnedResponse);
}

// Verify that GetBeforeViewSwitchAction() returns the correct action for
// the Android platform.
TEST_F(AndroidBnplStrategyTest, GetBeforeViewSwitchAction) {
  EXPECT_EQ(android_bnpl_strategy_.GetBeforeViewSwitchAction(),
            BnplStrategy::BeforeSwitchingViewAction::kDoNothing);
}

// Verify that ShouldRemoveExistingUiOnServerReturn() returns the correct
// value for the Android platform.
TEST_F(AndroidBnplStrategyTest, ShouldRemoveExistingUiOnServerReturn) {
  EXPECT_EQ(android_bnpl_strategy_.ShouldRemoveExistingUiOnServerReturn(
                PaymentsAutofillClient::PaymentsRpcResult::kSuccess),
            true);
  EXPECT_EQ(android_bnpl_strategy_.ShouldRemoveExistingUiOnServerReturn(
                PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure),
            false);
}

}  // namespace autofill::payments
