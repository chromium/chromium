// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/amount_extraction_manager.h"

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class MockAutofillDriver : public TestAutofillDriver {
 public:
  explicit MockAutofillDriver(TestAutofillClient* autofill_client)
      : TestAutofillDriver(autofill_client) {}

  MOCK_METHOD(void,
              ExtractLabeledTextNodeValue,
              (const std::u16string&,
               const std::u16string&,
               uint32_t,
               base::OnceCallback<void(const std::string&)>),
              (override));
};

class AmountExtractionManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    autofill_client_ = std::make_unique<TestAutofillClient>();
    mock_autofill_driver_ =
        std::make_unique<testing::StrictMock<MockAutofillDriver>>(
            autofill_client_.get());
    autofill_manager_ = std::make_unique<TestBrowserAutofillManager>(
        mock_autofill_driver_.get());
    amount_extraction_manager_ =
        std::make_unique<AmountExtractionManager>(autofill_manager_.get());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  std::unique_ptr<testing::StrictMock<MockAutofillDriver>>
      mock_autofill_driver_;
  std::unique_ptr<TestBrowserAutofillManager> autofill_manager_;
  std::unique_ptr<AmountExtractionManager> amount_extraction_manager_;
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(AmountExtractionManagerTest, ShouldTriggerWhenEligible) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableAmountExtractionDesktop};

  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;

  EXPECT_TRUE(amount_extraction_manager_->ShouldTriggerAmountExtraction(
      context, /*should_suppress_suggestions=*/false,
      /*has_suggestions=*/true));
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenFeatureIsNotEnabled) {
  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;

  EXPECT_FALSE(amount_extraction_manager_->ShouldTriggerAmountExtraction(
      context, /*should_suppress_suggestions=*/false,
      /*has_suggestions=*/true));
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenSearchIsOngoing) {
  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;
  amount_extraction_manager_->SetSearchRequestPendingForTesting(
      /*search_request_pending*/ true);
  EXPECT_FALSE(amount_extraction_manager_->ShouldTriggerAmountExtraction(
      context, /*should_suppress_suggestions=*/false,
      /*has_suggestions=*/true));
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenAutofillUnavailable) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableAmountExtractionDesktop};

  SuggestionsContext context;
  context.is_autofill_available = false;
  context.filling_product = FillingProduct::kCreditCard;

  EXPECT_FALSE(amount_extraction_manager_->ShouldTriggerAmountExtraction(
      context, /*should_suppress_suggestions=*/false,
      /*has_suggestions=*/true));
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenFormIsNotCreditCard) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableAmountExtractionDesktop};

  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kAddress;

  EXPECT_FALSE(amount_extraction_manager_->ShouldTriggerAmountExtraction(
      context, /*should_suppress_suggestions=*/false,
      /*has_suggestions=*/true));
}

TEST_F(AmountExtractionManagerTest,
       ShouldNotTriggerWhenSuggestionIsSuppressed) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableAmountExtractionDesktop};

  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;

  EXPECT_FALSE(amount_extraction_manager_->ShouldTriggerAmountExtraction(
      context, /*should_suppress_suggestions=*/true, /*has_suggestions=*/true));
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenNoSuggestion) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableAmountExtractionDesktop};

  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;

  EXPECT_FALSE(amount_extraction_manager_->ShouldTriggerAmountExtraction(
      context, /*should_suppress_suggestions=*/false,
      /*has_suggestions=*/false));
}

// This test checks when the search is triggered,
// `ExtractLabeledTextNodeValue` from `AutofillDriver` is invoked.
TEST_F(AmountExtractionManagerTest, TriggerCheckoutAmountExtraction) {
  EXPECT_CALL(
      *mock_autofill_driver_,
      ExtractLabeledTextNodeValue(
          base::UTF8ToUTF16(
              AmountExtractionHeuristicRegexes::GetInstance().amount_pattern()),
          base::UTF8ToUTF16(AmountExtractionHeuristicRegexes::GetInstance()
                                .keyword_pattern()),
          AmountExtractionHeuristicRegexes::GetInstance()
              .number_of_ancestor_levels_to_search(),
          testing::_))
      .Times(1);

  amount_extraction_manager_->TriggerCheckoutAmountExtraction();
}
#endif

}  // namespace autofill::payments
