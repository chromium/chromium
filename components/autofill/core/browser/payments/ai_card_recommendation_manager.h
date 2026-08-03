// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AI_CARD_RECOMMENDATION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AI_CARD_RECOMMENDATION_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"

namespace autofill {
class BrowserAutofillManager;
}  // namespace autofill

namespace autofill::payments {

// Owned by `BrowserAutofillManager`. This class manages the flow of AI card
// recommendation, which uses Gemini to recommend and reorder card
// suggestions based on the cards' benefits.
// This class is initialized when the user accepts the "Maximize rewards"
// suggestion, and is destroyed on user navigation or page refresh.
// TODO(crbug.com/539582738): Improve the lifecycle of this class to align with
// similar features.
class AiCardRecommendationManager : public AutofillManager::Observer {
 public:
  explicit AiCardRecommendationManager(
      BrowserAutofillManager* browser_autofill_manager);
  AiCardRecommendationManager(const AiCardRecommendationManager&) = delete;
  AiCardRecommendationManager& operator=(const AiCardRecommendationManager&) =
      delete;
  ~AiCardRecommendationManager() override;

  // Initializes the AI-based card recommendation flow, which includes calling
  // AI amount extraction and calling Gemini via `RemoteModelExecutor`.
  virtual void MaximizeCreditCardBenefits();

  // AutofillManager::Observer:
  void OnSuggestionsHidden(AutofillManager& manager,
                           SuggestionHidingReason reason) override;

  // Invoked once the AI-based amount extraction process completes, and
  // notifies AiCardRecommendationManager of the result.
  virtual void OnAmountExtractionReturnedFromAi(
      const AiAmountExtractionResult::ResultType result);

 private:
  // The owner, BrowserAutofillManager.
  const raw_ref<BrowserAutofillManager> browser_autofill_manager_;

  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      autofill_manager_observation_{this};

  base::WeakPtrFactory<AiCardRecommendationManager> weak_ptr_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AI_CARD_RECOMMENDATION_MANAGER_H_
