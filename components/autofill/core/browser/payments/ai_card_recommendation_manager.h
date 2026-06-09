// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AI_CARD_RECOMMENDATION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AI_CARD_RECOMMENDATION_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

namespace autofill {
class BrowserAutofillManager;
}  // namespace autofill

namespace autofill::payments {

// Owned by `BrowserAutofillManager`. This class manages the flow of AI card
// recommendation, which uses Gemini to recommend and reorder card
// suggestions based on the cards' benefits.
class AiCardRecommendationManager {
 public:
  explicit AiCardRecommendationManager(
      BrowserAutofillManager* browser_autofill_manager);
  AiCardRecommendationManager(const AiCardRecommendationManager&) = delete;
  AiCardRecommendationManager& operator=(const AiCardRecommendationManager&) =
      delete;
  ~AiCardRecommendationManager();

 private:
  // The owner, BrowserAutofillManager.
  const raw_ref<BrowserAutofillManager> browser_autofill_manager_;

  base::WeakPtrFactory<AiCardRecommendationManager> weak_ptr_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AI_CARD_RECOMMENDATION_MANAGER_H_
