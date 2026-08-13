// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/ai_card_recommendation_manager.h"

#include <ranges>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::payments {

AiCardRecommendationManager::AiCardRecommendationManager(
    BrowserAutofillManager* browser_autofill_manager)
    : browser_autofill_manager_(CHECK_DEREF(browser_autofill_manager)) {
  autofill_manager_observation_.Observe(&*browser_autofill_manager_);
}

AiCardRecommendationManager::~AiCardRecommendationManager() = default;

// static
bool AiCardRecommendationManager::
    ShouldShowMaximizeCreditCardBenefitsSuggestion(
        const std::vector<CreditCard>& cards_to_suggest,
        bool is_card_number_field_empty) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableAiCardRecommendation)) {
    return false;
  }
  if (!is_card_number_field_empty) {
    return false;
  }
  size_t eligible_cards_count =
      std::ranges::count_if(cards_to_suggest, [](const CreditCard& card) {
        return !card.product_description().empty();
      });
  return eligible_cards_count >= 2;
}

void AiCardRecommendationManager::MaximizeCreditCardBenefits() {
  browser_autofill_manager_->GetAmountExtractionManager()
      .TriggerCheckoutAmountExtractionWithAi(base::BindOnce(
          &AiCardRecommendationManager::OnAmountExtractionReturnedFromAi,
          weak_ptr_factory_.GetWeakPtr()));
}

void AiCardRecommendationManager::OnSuggestionsHidden(AutofillManager&,
                                                      SuggestionHidingReason) {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void AiCardRecommendationManager::OnAmountExtractionReturnedFromAi(
    const AiAmountExtractionResult::ResultType result) {
  // TODO(crbug.com/524295951): Handle card recommendation using extracted
  // amount or fallback on error.
}

}  // namespace autofill::payments
