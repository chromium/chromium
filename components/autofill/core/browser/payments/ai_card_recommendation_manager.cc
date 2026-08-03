// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/ai_card_recommendation_manager.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"

namespace autofill::payments {

AiCardRecommendationManager::AiCardRecommendationManager(
    BrowserAutofillManager* browser_autofill_manager)
    : browser_autofill_manager_(CHECK_DEREF(browser_autofill_manager)) {
  autofill_manager_observation_.Observe(&*browser_autofill_manager_);
}

AiCardRecommendationManager::~AiCardRecommendationManager() = default;

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
