// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"

#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/filling/test_form_filler.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/payments/test/mock_ai_card_recommendation_manager.h"
#include "components/autofill/core/browser/payments/test/mock_bnpl_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

namespace autofill {

TestBrowserAutofillManager::TestBrowserAutofillManager(AutofillDriver* driver)
    : TestAutofillManagerTemplate<BrowserAutofillManager>(driver) {
  test_api(*this).set_form_filler(std::make_unique<TestFormFiller>(*this));
}

TestBrowserAutofillManager::~TestBrowserAutofillManager() = default;

testing::NiceMock<MockBnplManager>*
TestBrowserAutofillManager::GetPaymentsBnplManager() {
  return &mock_bnpl_manager_;
}

testing::NiceMock<MockAiCardRecommendationManager>&
TestBrowserAutofillManager::GetAiCardRecommendationManager() {
  return mock_ai_card_recommendation_manager_;
}

const gfx::Image& TestBrowserAutofillManager::GetCardImage(
    const CreditCard& credit_card) {
  return card_image_;
}

}  // namespace autofill
