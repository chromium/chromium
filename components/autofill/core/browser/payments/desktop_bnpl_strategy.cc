// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/desktop_bnpl_strategy.h"

#include "base/feature_list.h"
#include "components/autofill/core/browser/payments/bnpl_strategy.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::payments {

DesktopBnplStrategy::DesktopBnplStrategy() = default;

DesktopBnplStrategy::~DesktopBnplStrategy() = default;

BnplStrategy::SuggestionsShownNextAction
DesktopBnplStrategy::GetNextActionOnSuggestionsShown() {
  return SuggestionsShownNextAction::
      kNotifyUpdateCallbackOfSuggestionsShownResponse;
}

BnplStrategy::UserDecisionToUseBnplNextAction
DesktopBnplStrategy::GetNextActionOnUserDecisionToUseBnpl() {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnablePayNowPayLaterTabs)) {
    return UserDecisionToUseBnplNextAction::kDoNothing;
  }

  return UserDecisionToUseBnplNextAction::kShowSelectBnplIssuerUiForDesktop;
}

BnplStrategy::UserDecisionToUseBnplAgainNextAction
DesktopBnplStrategy::GetNextActionOnUserDecisionToUseBnplAgain() {
  return UserDecisionToUseBnplAgainNextAction::kDoNothing;
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

BnplStrategy::BnplAiBasedAmountExtractionReturnedNextAction
DesktopBnplStrategy::GetNextActionOnAiBasedAmountExtractionReturned() {
  return BnplAiBasedAmountExtractionReturnedNextAction::
      kReplaceLoadingThrobberWithIssuerSuggestionsOnDesktop;
}

BnplStrategy::UserDecisionToUseSavedCardsNextAction
DesktopBnplStrategy::GetNextActionOnUserDecisionToUseSavedCards() {
  return UserDecisionToUseSavedCardsNextAction::kUpdateDesktopPopupSuggestions;
}

BnplStrategy::UiDismissalAction DesktopBnplStrategy::GetUiDismissalAction() {
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnablePayNowPayLaterTabs)) {
    return UiDismissalAction::kHideSuggestions;
  }
  return UiDismissalAction::kRemoveBnplUi;
}

bool DesktopBnplStrategy::ShouldRemoveExistingUiOnServerReturn(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  return true;
}

}  // namespace autofill::payments
