// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/amount_extraction_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/metrics/payments/amount_extraction_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/test/mock_bnpl_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

namespace {
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Test;
}  // namespace

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

class MockAmountExtractionManager : public AmountExtractionManager {
 public:
  explicit MockAmountExtractionManager(BrowserAutofillManager* autofill_manager)
      : AmountExtractionManager(autofill_manager) {}

  MOCK_METHOD(void,
              OnCheckoutAmountReceived,
              (base::TimeTicks search_request_start_timestamp,
               const std::string& extracted_amount),
              (override));
  MOCK_METHOD(void, OnTimeoutReached, (), (override));
};

class AmountExtractionManagerTest : public Test {
 public:
  AmountExtractionManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillEnableAmountExtractionDesktop,
                              features::kAutofillEnableBuyNowPayLaterSyncing,
                              features::kAutofillEnableBuyNowPayLater},
        /*disabled_features=*/{
            features::kAutofillEnableAmountExtractionDesktopLogging});
  }

 protected:
  void SetUp() override {
    autofill_client_ = std::make_unique<TestAutofillClient>();
    autofill_client_->SetAutofillPaymentMethodsEnabled(true);
    autofill_client_->GetPersonalDataManager()
        .payments_data_manager()
        .SetSyncingForTest(true);
    autofill_client_->GetPersonalDataManager().SetPrefService(
        autofill_client_->GetPrefs());
    mock_autofill_driver_ =
        std::make_unique<NiceMock<MockAutofillDriver>>(autofill_client_.get());
    autofill_manager_ = std::make_unique<TestBrowserAutofillManager>(
        mock_autofill_driver_.get());
    amount_extraction_manager_ =
        std::make_unique<AmountExtractionManager>(autofill_manager_.get());

    test_api(payments_data()).AddBnplIssuer(test::GetTestUnlinkedBnplIssuer());

    ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                autofill_manager_->client().GetAutofillOptimizationGuide()),
            IsUrlEligibleForBnplIssuer)
        .WillByDefault(Return(true));
  }

  TestPaymentsDataManager& payments_data() {
    return autofill_client_->GetPersonalDataManager()
        .test_payments_data_manager();
  }

  void FakeCheckoutAmountReceived(const std::string& extracted_amount) {
    amount_extraction_manager_->OnCheckoutAmountReceived(base::TimeTicks::Now(),
                                                         extracted_amount);
  }

  void SetUpCheckoutAmountExtractionCall(const std::string& extracted_amount,
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
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  std::unique_ptr<NiceMock<MockAutofillDriver>> mock_autofill_driver_;
  std::unique_ptr<TestBrowserAutofillManager> autofill_manager_;
  std::unique_ptr<AmountExtractionManager> amount_extraction_manager_;
  std::unique_ptr<MockAmountExtractionManager> mock_amount_extraction_manager_;
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(AmountExtractionManagerTest, ShouldTriggerWhenEligible) {
  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;
  std::vector<FieldType> field_types = {FieldType::CREDIT_CARD_NUMBER,
                                        FieldType::CREDIT_CARD_NAME_FULL,
                                        FieldType::CREDIT_CARD_EXP_MONTH};

  for (FieldType field_type : field_types) {
    EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                    context,
                    /*should_suppress_suggestions=*/false,
                    /*has_suggestions=*/true,
                    /*field_type=*/field_type),
                ElementsAre(AmountExtractionManager::EligibleFeature::kBnpl));
  }
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenCvcFieldIsClicked) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableAmountExtractionDesktop};

  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;

  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  context, /*should_suppress_suggestions=*/false,
                  /*has_suggestions=*/true,
                  /*field_type=*/FieldType::CREDIT_CARD_VERIFICATION_CODE),
              IsEmpty());
  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  context, /*should_suppress_suggestions=*/false,
                  /*has_suggestions=*/true,
                  /*field_type=*/
                  FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE),
              IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenFeatureIsNotEnabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{features::kAutofillEnableAmountExtractionDesktop});

  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;

  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  context, /*should_suppress_suggestions=*/false,
                  /*has_suggestions=*/true,
                  /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
              IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenSearchIsOngoing) {
  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;
  amount_extraction_manager_->SetSearchRequestPendingForTesting(
      /*search_request_pending*/ true);
  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  context, /*should_suppress_suggestions=*/false,
                  /*has_suggestions=*/true,
                  /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
              IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenAutofillUnavailable) {
  SuggestionsContext context;
  context.is_autofill_available = false;
  context.filling_product = FillingProduct::kCreditCard;

  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  context, /*should_suppress_suggestions=*/false,
                  /*has_suggestions=*/true,
                  /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
              IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenFormIsNotCreditCard) {
  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kAddress;

  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  context, /*should_suppress_suggestions=*/false,
                  /*has_suggestions=*/true,
                  /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
              IsEmpty());
}

TEST_F(AmountExtractionManagerTest,
       ShouldNotTriggerWhenSuggestionIsSuppressed) {
  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;

  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  context, /*should_suppress_suggestions=*/true,
                  /*has_suggestions=*/true,
                  /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
              IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenNoSuggestion) {
  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;

  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  context, /*should_suppress_suggestions=*/false,
                  /*has_suggestions=*/false,
                  /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
              IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerIfUrlNotEligible) {
  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_manager_->client().GetAutofillOptimizationGuide()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(false));

  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  context, /*should_suppress_suggestions=*/false,
                  /*has_suggestions=*/true,
                  /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
              IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerInIncognitoMode) {
  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;
  std::vector<FieldType> field_types = {FieldType::CREDIT_CARD_NUMBER,
                                        FieldType::CREDIT_CARD_NAME_FULL,
                                        FieldType::CREDIT_CARD_EXP_MONTH};
  autofill_client_->set_is_off_the_record(/*is_off_the_record=*/true);

  for (FieldType field_type : field_types) {
    EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                    context, /*should_suppress_suggestions=*/false,
                    /*has_suggestions=*/true,
                    /*field_type=*/field_type),
                IsEmpty());
  }
}

TEST_F(AmountExtractionManagerTest, ShouldTriggerWhenLoggingFeatureIsEnabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableAmountExtractionDesktop,
                            features::
                                kAutofillEnableAmountExtractionDesktopLogging},
      /*disabled_features=*/{});
  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_manager_->client().GetAutofillOptimizationGuide()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(false));

  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  context, /*should_suppress_suggestions=*/false,
                  /*has_suggestions=*/true,
                  /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
              ElementsAre(AmountExtractionManager::EligibleFeature::kBnpl));
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerIfNoBnplIssuer) {
  SuggestionsContext context;
  context.is_autofill_available = true;
  context.filling_product = FillingProduct::kCreditCard;
  payments_data().ClearBnplIssuers();

  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  context, /*should_suppress_suggestions=*/false,
                  /*has_suggestions=*/true,
                  /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
              IsEmpty());
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
          _))
      .Times(1);

  amount_extraction_manager_->TriggerCheckoutAmountExtraction();
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to monetary values they represent in micro-units when given empty
// string or zeros.
TEST_F(AmountExtractionManagerTest, AmountParser_Zeros) {
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(""),
            std::nullopt);
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("$0"),
            std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("$0.00"),
      0ULL);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to monetary values they represent in micro-units when given normal
// format of strings.
TEST_F(AmountExtractionManagerTest, AmountParser_NormalCases) {
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("$ 12.34"),
      12'340'000ULL);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("$ 012.34"),
      12'340'000ULL);
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "USD 1,234.56"),
            1'234'560'000ULL);
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "$ 1,234.56"),
            1'234'560'000ULL);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("$ 123.45"),
      123'450'000ULL);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("$0.12"),
      120'000ULL);
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "USD   0.12"),
            120'000ULL);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to monetary values they represent in micro-units when given input
// string with leading and tailing monetary-representing substrings.
TEST_F(AmountExtractionManagerTest, AmountParser_LeadingAndTailingCharacters) {
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "$   1,234.56   USD"),
            1'234'560'000ULL);
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "USD $ 1,234.56 USD"),
            1'234'560'000ULL);
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "  $ 1,234.56 "),
            1'234'560'000ULL);
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "USD    1234.56    "),
            1'234'560'000ULL);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to std::nullopt when given negative value strings.
TEST_F(AmountExtractionManagerTest, AmountParser_NegativeValue) {
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "$ -1,234.56"),
            std::nullopt);
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "USD -1,234.56"),
            std::nullopt);
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "USD 1,234.56- $"),
            std::nullopt);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to std::nullopt when given incorrect format of strings.
TEST_F(AmountExtractionManagerTest, AmountParser_IncorrectFormatOfInputs) {
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "$ ,123.45"),
            std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("$1,234.5"),
      std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("NaN"),
      std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("Inf"),
      std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("-Inf"),
      std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("1.234E8"),
      std::nullopt);
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "$1.234.56"),
            std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("$ 12e2"),
      std::nullopt);
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "$ 12e2.23"),
            std::nullopt);
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "$ 12.23e2"),
            std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("E1.23"),
      std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("E1.23"),
      std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("e1.23"),
      std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("-1.23"),
      std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("1.23E"),
      std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("1.23e"),
      std::nullopt);
  EXPECT_EQ(
      AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits("1.23-"),
      std::nullopt);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to std::nullopt when the converted value overflows uint64.
TEST_F(AmountExtractionManagerTest, AmountParser_OverflowValue) {
  EXPECT_EQ(AmountExtractionManager::MaybeParseAmountToMonetaryMicroUnits(
                "$19000000000000.00"),
            std::nullopt);
}

TEST_F(AmountExtractionManagerTest,
       TriggerCheckoutAmountExtraction_Success_Metric) {
  constexpr int kDefaultAmountExtractionLatencyMs = 200;
  constexpr std::string kExtractedAmount = "123.45";
  base::HistogramTester histogram_tester;
  SetUpCheckoutAmountExtractionCall(
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
          _))
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
  SetUpCheckoutAmountExtractionCall(
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
          _))
      .Times(1);

  amount_extraction_manager_->TriggerCheckoutAmountExtraction();
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.AmountExtraction.Latency.Failure",
      base::Milliseconds(kDefaultAmountExtractionLatencyMs), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.AmountExtraction.Latency",
      base::Milliseconds(kDefaultAmountExtractionLatencyMs), 1);
}

// Verify that Amount extraction records true for a successful extraction.
TEST_F(AmountExtractionManagerTest, AmountExtractionResult_Metric_Successful) {
  constexpr std::string kExtractedAmount = "123.45";
  base::HistogramTester histogram_tester;
  SetUpCheckoutAmountExtractionCall(
      /*extracted_amount=*/kExtractedAmount,
      /*latency_ms=*/0);
  EXPECT_CALL(
      *mock_autofill_driver_,
      ExtractLabeledTextNodeValue(
          base::UTF8ToUTF16(
              AmountExtractionHeuristicRegexes::GetInstance().amount_pattern()),
          base::UTF8ToUTF16(AmountExtractionHeuristicRegexes::GetInstance()
                                .keyword_pattern()),
          AmountExtractionHeuristicRegexes::GetInstance()
              .number_of_ancestor_levels_to_search(),
          _))
      .Times(1);

  amount_extraction_manager_->TriggerCheckoutAmountExtraction();
  histogram_tester.ExpectUniqueSample(
      "Autofill.AmountExtraction.Result",
      autofill::autofill_metrics::AmountExtractionResult::kSuccessful, 1);
}

// Verify that Amount extraction records false for a failed extraction.
TEST_F(AmountExtractionManagerTest,
       AmountExtractionResult_Metric_AmountNotFound) {
  base::HistogramTester histogram_tester;
  SetUpCheckoutAmountExtractionCall(
      /*extracted_amount=*/"",
      /*latency_ms=*/0);
  EXPECT_CALL(
      *mock_autofill_driver_,
      ExtractLabeledTextNodeValue(
          base::UTF8ToUTF16(
              AmountExtractionHeuristicRegexes::GetInstance().amount_pattern()),
          base::UTF8ToUTF16(AmountExtractionHeuristicRegexes::GetInstance()
                                .keyword_pattern()),
          AmountExtractionHeuristicRegexes::GetInstance()
              .number_of_ancestor_levels_to_search(),
          _))
      .Times(1);

  amount_extraction_manager_->TriggerCheckoutAmountExtraction();
  histogram_tester.ExpectUniqueSample(
      "Autofill.AmountExtraction.Result",
      autofill::autofill_metrics::AmountExtractionResult::kAmountNotFound, 1);
}

TEST_F(AmountExtractionManagerTest, AmountExtractionResult_Metric_Timeout) {
  base::HistogramTester histogram_tester;
  ON_CALL(*mock_autofill_driver_, ExtractLabeledTextNodeValue)
      .WillByDefault(
          [this](const std::u16string&, const std::u16string&, uint32_t,
                 base::OnceCallback<void(const std::string&)>&& callback) {
            task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
                FROM_HERE, base::BindOnce(std::move(callback), "123"),
                AmountExtractionManager::kAmountExtractionWaitTime +
                    base::Milliseconds(100));
          });

  amount_extraction_manager_->TriggerCheckoutAmountExtraction();
  task_environment_.FastForwardBy(
      AmountExtractionManager::kAmountExtractionWaitTime);

  histogram_tester.ExpectUniqueSample(
      "Autofill.AmountExtraction.Result",
      autofill::autofill_metrics::AmountExtractionResult::kTimeout, 1);
}

TEST_F(AmountExtractionManagerTest, TimeoutExpiresBeforeResponse) {
  mock_amount_extraction_manager_ =
      std::make_unique<MockAmountExtractionManager>(autofill_manager_.get());
  EXPECT_FALSE(
      mock_amount_extraction_manager_->GetSearchRequestPendingForTesting());
  EXPECT_CALL(*mock_autofill_driver_, ExtractLabeledTextNodeValue)
      .WillOnce(
          [this](const std::u16string&, const std::u16string&, uint32_t,
                 base::OnceCallback<void(const std::string&)>&& callback) {
            task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
                FROM_HERE, base::BindOnce(std::move(callback), "123"),
                AmountExtractionManager::kAmountExtractionWaitTime +
                    base::Milliseconds(100));
          });
  EXPECT_CALL(*mock_amount_extraction_manager_, OnCheckoutAmountReceived)
      .Times(0);
  EXPECT_CALL(*mock_amount_extraction_manager_, OnTimeoutReached()).Times(1);
  mock_amount_extraction_manager_->TriggerCheckoutAmountExtraction();
  task_environment_.RunUntilIdle();
  task_environment_.FastForwardBy(
      AmountExtractionManager::kAmountExtractionWaitTime);
}

TEST_F(AmountExtractionManagerTest, ResponseBeforeTimeout) {
  mock_amount_extraction_manager_ =
      std::make_unique<MockAmountExtractionManager>(autofill_manager_.get());
  EXPECT_FALSE(
      mock_amount_extraction_manager_->GetSearchRequestPendingForTesting());
  EXPECT_CALL(*mock_autofill_driver_, ExtractLabeledTextNodeValue)
      .WillOnce(
          [this](const std::u16string&, const std::u16string&, uint32_t,
                 base::OnceCallback<void(const std::string&)>&& callback) {
            task_environment_.FastForwardBy(
                AmountExtractionManager::kAmountExtractionWaitTime / 2);
            std::move(callback).Run("123");
          });
  EXPECT_CALL(*mock_amount_extraction_manager_,
              OnCheckoutAmountReceived(_, Eq("123")))
      .Times(1);
  EXPECT_CALL(*mock_amount_extraction_manager_, OnTimeoutReached()).Times(0);
  mock_amount_extraction_manager_->TriggerCheckoutAmountExtraction();
  task_environment_.RunUntilIdle();
}

// This test checks that the BNPL manager will be notified when the amount
// extraction receives a empty result.
TEST_F(AmountExtractionManagerTest,
       OnCheckoutAmountReceived_EmptyResult_BnplManagerNotified) {
  EXPECT_CALL(*autofill_manager_->GetPaymentsBnplManager(),
              OnAmountExtractionReturned(std::optional<uint64_t>()))
      .Times(1);

  FakeCheckoutAmountReceived("");
}

// This test checks that the BNPL manager will be notified when the amount
// extraction receives a result with correct format.
TEST_F(AmountExtractionManagerTest,
       OnCheckoutAmountReceived_AmountInCorrectFormat_BnplManagerNotified) {
  EXPECT_CALL(
      *autofill_manager_->GetPaymentsBnplManager(),
      OnAmountExtractionReturned(std::optional<uint64_t>(123'450'000ULL)))
      .Times(1);

  FakeCheckoutAmountReceived("$ 123.45");
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace autofill::payments
