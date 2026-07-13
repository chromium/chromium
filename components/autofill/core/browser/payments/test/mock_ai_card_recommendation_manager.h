// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_AI_CARD_RECOMMENDATION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_AI_CARD_RECOMMENDATION_MANAGER_H_

#include "components/autofill/core/browser/payments/ai_card_recommendation_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAiCardRecommendationManager
    : public payments::AiCardRecommendationManager {
 public:
  explicit MockAiCardRecommendationManager(
      BrowserAutofillManager* browser_autofill_manager);
  ~MockAiCardRecommendationManager() override;

  MOCK_METHOD(void, MaximizeCreditCardBenefits, (), (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_MOCK_AI_CARD_RECOMMENDATION_MANAGER_H_
