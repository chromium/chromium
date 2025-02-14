// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/amount_extraction_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/metrics/payments/amount_extraction_metrics.h"
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
    ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                autofill_manager_->client().GetAutofillOptimizationGuide()),
            IsUrlEligibleForCheckoutAmountSearchForIssuerId)
        .WillByDefault(testing::Return(true));
  }

  void SetUpExtractLabeledTextNodeValue(const std::string& extracted_amount,
                                        int latency_ms = 0) {
    auto extract_action =
        [=, this](const std::u16string& /*amount_pattern*/,
                  const std::u16string& /*keyword_pattern*/,
                  uint32_t /*ancestor_levels*/,
                  base::OnceCallback<void(const std::string&)> callback) {
          task_environment_.FastForwardBy(base::Milliseconds(latency_ms));
          std::move(callback).Run(extracted_amount);
        };
    ON_CALL(*mock_autofill_driver_, ExtractLabeledTextNodeValue)
        .WillByDefault(std::move(extract_action));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
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

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerIfUrlNotEligible) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableAmountExtractionDesktop};

  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_manager_->client().GetAutofillOptimizationGuide()),
          IsUrlEligibleForCheckoutAmountSearchForIssuerId)
      .WillByDefault(testing::Return(false));

  EXPECT_FALSE(amount_extraction_manager_->ShouldTriggerAmountExtraction(
      context, /*should_suppress_suggestions=*/false,
      /*has_suggestions=*/true));
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

TEST_F(AmountExtractionManagerTest,
       TriggerCheckoutAmountExtraction_Success_Metric) {
  constexpr int kDefaultAmountExtractionLatencyMs = 200;
  constexpr std::string kExtractedAmount = "123.45";
  base::HistogramTester histogram_tester;
  SetUpExtractLabeledTextNodeValue(
      /*extracted_amount=*/kExtractedAmount,
      /*latency_ms=*/kDefaultAmountExtractionLatencyMs);
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
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.AmountExtraction.Latency.Success",
      base::Milliseconds(kDefaultAmountExtractionLatencyMs), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.AmountExtraction.Latency",
      base::Milliseconds(kDefaultAmountExtractionLatencyMs), 1);
}

TEST_F(AmountExtractionManagerTest,
       TriggerCheckoutAmountExtraction_Failure_Metric) {
  constexpr int kDefaultAmountExtractionLatencyMs = 200;
  base::HistogramTester histogram_tester;
  SetUpExtractLabeledTextNodeValue(
      /*extracted_amount=*/"",
      /*latency_ms=*/kDefaultAmountExtractionLatencyMs);
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
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.AmountExtraction.Latency.Failure",
      base::Milliseconds(kDefaultAmountExtractionLatencyMs), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.AmountExtraction.Latency",
      base::Milliseconds(kDefaultAmountExtractionLatencyMs), 1);
}

#endif

}  // namespace autofill::payments
