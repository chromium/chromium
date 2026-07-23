// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_BROWSER_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_BROWSER_AUTOFILL_MANAGER_H_

#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager.h"
#include "components/autofill/core/browser/payments/test/mock_ai_card_recommendation_manager.h"
#include "components/autofill/core/browser/payments/test/mock_bnpl_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {

class MockBnplManager;
class AutofillDriver;

class TestBrowserAutofillManager
    : public TestAutofillManagerTemplate<BrowserAutofillManager> {
 public:
  explicit TestBrowserAutofillManager(AutofillDriver* driver);
  TestBrowserAutofillManager(const TestBrowserAutofillManager&) = delete;
  TestBrowserAutofillManager& operator=(const TestBrowserAutofillManager&) =
      delete;
  ~TestBrowserAutofillManager() override;

  // BrowserAutofillManager:
  const gfx::Image& GetCardImage(const CreditCard& credit_card) override;
  testing::NiceMock<MockBnplManager>* GetPaymentsBnplManager() override;
  testing::NiceMock<MockAiCardRecommendationManager>&
  GetAiCardRecommendationManager() override;

 private:
  const gfx::Image card_image_ = gfx::test::CreateImage(40, 24);

  testing::NiceMock<MockBnplManager> mock_bnpl_manager_{this};
  testing::NiceMock<MockAiCardRecommendationManager>
      mock_ai_card_recommendation_manager_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_BROWSER_AUTOFILL_MANAGER_H_
