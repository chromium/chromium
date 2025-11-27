// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/amount_extraction_manager.h"

#include <cmath>
#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/mock_autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/metrics/payments/amount_extraction_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager_test_api.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/test/mock_bnpl_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/amount_extraction.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

namespace {
using base::test::EqualsProto;
using ::testing::_;
using ::testing::A;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Test;
using ModelExecutionCallback = base::OnceCallback<void(
    optimization_guide::OptimizationGuideModelExecutionResult,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry>)>;
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

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(
      void,
      GetAiPageContent,
      (base::OnceCallback<void(
           std::optional<optimization_guide::proto::AnnotatedPageContent>)>),
      (override));
  MOCK_METHOD(optimization_guide::RemoteModelExecutor*,
              GetRemoteModelExecutor,
              (),
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

class AmountExtractionManagerTest
    : public Test,
      public WithTestAutofillClientDriverManager<MockAutofillClient,
                                                 MockAutofillDriver> {
 public:
  AmountExtractionManagerTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kAutofillEnableAmountExtraction,
        features::kAutofillEnableBuyNowPayLaterSyncing,
        features::kAutofillEnableBuyNowPayLater};
    std::vector<base::test::FeatureRef> disabled_features = {
        features::kAutofillEnableAmountExtractionTesting};
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
  void SetUp() override {
    InitAutofillClient();
    payments_autofill_client().SetAutofillPaymentMethodsEnabled(true);
    autofill_client()
        .GetPersonalDataManager()
        .payments_data_manager()
        .SetSyncingForTest(true);
    CreateAutofillDriver();
    amount_extraction_manager_ =
        std::make_unique<AmountExtractionManager>(&autofill_manager());

    test_api(payments_data()).AddBnplIssuer(test::GetTestUnlinkedBnplIssuer());

    ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
                autofill_client().GetAutofillOptimizationGuideDecider()),
            IsUrlEligibleForBnplIssuer)
        .WillByDefault(Return(true));

    ON_CALL(autofill_client(), GetRemoteModelExecutor())
        .WillByDefault(Return(model_executor()));
  }

  TestPaymentsDataManager& payments_data() {
    return autofill_client()
        .GetPersonalDataManager()
        .test_payments_data_manager();
  }

  void FakeCheckoutAmountReceived(const std::string& extracted_amount) {
    amount_extraction_manager_->OnCheckoutAmountReceived(base::TimeTicks::Now(),
                                                         extracted_amount);
  }

  void FakeAmountExtractionTimeout() {
    test_api(*amount_extraction_manager_).SetSearchRequestPending(true);
    amount_extraction_manager_->OnTimeoutReached();
  }

  void FakeCheckoutAmountReceivedFromAi(
      const std::double_t& final_checkout_amount,
      const std::string& currency,
      const bool is_successful) {
    optimization_guide::proto::AmountExtractionResponse response;
    response.set_final_checkout_amount(final_checkout_amount);
    response.set_currency(currency);
    response.set_is_successful(is_successful);
    std::string serialized_metadata;
    response.SerializeToString(&serialized_metadata);
    optimization_guide::proto::Any any_result;
    any_result.set_type_url(
        base::StrCat({"type.googleapis.com/", response.GetTypeName()}));
    any_result.set_value(serialized_metadata);

    amount_extraction_manager_->OnCheckoutAmountReceivedFromAi(
        optimization_guide::OptimizationGuideModelExecutionResult(any_result,
                                                                  nullptr),
        nullptr);
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
    ON_CALL(autofill_driver(), ExtractLabeledTextNodeValue)
        .WillByDefault(std::move(extract_action));
  }

  NiceMock<optimization_guide::MockRemoteModelExecutor>* model_executor() {
    return &mock_model_executor_;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AmountExtractionManager> amount_extraction_manager_;
  std::unique_ptr<MockAmountExtractionManager> mock_amount_extraction_manager_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  NiceMock<optimization_guide::MockRemoteModelExecutor> mock_model_executor_;
};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(AmountExtractionManagerTest, ShouldTriggerWhenEligible) {
  std::vector<FieldType> field_types = {FieldType::CREDIT_CARD_NUMBER,
                                        FieldType::CREDIT_CARD_NAME_FULL,
                                        FieldType::CREDIT_CARD_EXP_MONTH};

  for (FieldType field_type : field_types) {
    EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                    /*is_autofill_payments_enabled=*/true,
                    /*should_suppress_suggestions=*/false,
                    /*suggestions=*/
                    std::vector<Suggestion>{
                        Suggestion(SuggestionType::kCreditCardEntry)},
                    /*filling_product=*/FillingProduct::kCreditCard,
                    /*field_type=*/field_type),
                ElementsAre(AmountExtractionManager::EligibleFeature::kBnpl));
  }
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenCvcFieldIsClicked) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableAmountExtraction};

  EXPECT_THAT(
      amount_extraction_manager_->GetEligibleFeatures(
          /*is_autofill_payments_enabled=*/true,
          /*should_suppress_suggestions=*/false,
          /*suggestions=*/
          std::vector<Suggestion>{Suggestion(SuggestionType::kCreditCardEntry)},
          /*filling_product=*/FillingProduct::kCreditCard,
          /*field_type=*/FieldType::CREDIT_CARD_VERIFICATION_CODE),
      IsEmpty());
  EXPECT_THAT(
      amount_extraction_manager_->GetEligibleFeatures(
          /*is_autofill_payments_enabled=*/true,
          /*should_suppress_suggestions=*/false,
          /*suggestions=*/
          std::vector<Suggestion>{Suggestion(SuggestionType::kCreditCardEntry)},
          /*filling_product=*/FillingProduct::kCreditCard, /*field_type=*/
          FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE),
      IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenFeatureIsNotEnabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{features::kAutofillEnableAmountExtraction});

  EXPECT_THAT(
      amount_extraction_manager_->GetEligibleFeatures(
          /*is_autofill_payments_enabled=*/true,
          /*should_suppress_suggestions=*/false,
          /*suggestions=*/
          std::vector<Suggestion>{Suggestion(SuggestionType::kCreditCardEntry)},
          /*filling_product=*/FillingProduct::kCreditCard,
          /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
      IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenSearchIsOngoing) {
  test_api(*amount_extraction_manager_)
      .SetSearchRequestPending(
          /*search_request_pending*/ true);
  EXPECT_THAT(
      amount_extraction_manager_->GetEligibleFeatures(
          /*is_autofill_payments_enabled=*/true,
          /*should_suppress_suggestions=*/false,
          /*suggestions=*/
          std::vector<Suggestion>{Suggestion(SuggestionType::kCreditCardEntry)},
          /*filling_product=*/FillingProduct::kCreditCard,
          /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
      IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenAutofillUnavailable) {
  EXPECT_THAT(
      amount_extraction_manager_->GetEligibleFeatures(
          /*is_autofill_payments_enabled=*/false,
          /*should_suppress_suggestions=*/false,
          /*suggestions=*/
          std::vector<Suggestion>{Suggestion(SuggestionType::kCreditCardEntry)},
          /*filling_product=*/FillingProduct::kCreditCard,
          /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
      IsEmpty());
}

TEST_F(AmountExtractionManagerTest,
       AiBasedAmountExtractionShouldNotTriggerWhenNoBnplSuggestion) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableAiBasedAmountExtraction};
  EXPECT_THAT(
      amount_extraction_manager_->GetEligibleFeatures(
          /*is_autofill_payments_enabled=*/true,
          /*should_suppress_suggestions=*/false,
          /*suggestions=*/
          std::vector<Suggestion>{Suggestion(SuggestionType::kCreditCardEntry)},
          /*filling_product=*/FillingProduct::kCreditCard,
          /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
      IsEmpty());
}

TEST_F(AmountExtractionManagerTest,
       AiBasedAmountExtractionShouldNotTriggerWhenAutofillDisabled) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableAiBasedAmountExtraction};
  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  /*is_autofill_payments_enabled=*/false,
                  /*should_suppress_suggestions=*/false,
                  /*suggestions=*/std::vector<Suggestion>{},
                  /*filling_product=*/FillingProduct::kCreditCard,
                  /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
              IsEmpty());
}

TEST_F(AmountExtractionManagerTest,
       AiBasedAmountExtractionShouldTriggerWhenBnplSuggestionPresent) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableAiBasedAmountExtraction};

  EXPECT_THAT(
      amount_extraction_manager_->GetEligibleFeatures(
          /*is_autofill_payments_enabled=*/true,
          /*should_suppress_suggestions=*/false,
          /*suggestions=*/
          std::vector<Suggestion>{Suggestion(SuggestionType::kBnplEntry)},
          /*filling_product=*/FillingProduct::kCreditCard,
          /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
      // Verifies the set contains exactly this one element
      testing::UnorderedElementsAre(
          AmountExtractionManager::EligibleFeature::kBnpl));
}

TEST_F(
    AmountExtractionManagerTest,
    AiBasedAmountExtractionShouldTriggerWhenBnplSuggestionPresentButFeatureDisabled) {
  EXPECT_THAT(
      amount_extraction_manager_->GetEligibleFeatures(
          /*is_autofill_payments_enabled=*/true,
          /*should_suppress_suggestions=*/false,
          /*suggestions=*/
          std::vector<Suggestion>{Suggestion(SuggestionType::kBnplEntry)},
          /*filling_product=*/FillingProduct::kCreditCard,
          /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
      // Verifies the set contains exactly this one element
      testing::UnorderedElementsAre(
          AmountExtractionManager::EligibleFeature::kBnpl));
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenFormIsNotCreditCard) {
  EXPECT_THAT(
      amount_extraction_manager_->GetEligibleFeatures(
          /*is_autofill_payments_enabled=*/true,
          /*should_suppress_suggestions=*/false,
          /*suggestions=*/
          std::vector<Suggestion>{Suggestion(SuggestionType::kCreditCardEntry)},
          /*filling_product=*/FillingProduct::kAddress,
          /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
      IsEmpty());
}

TEST_F(AmountExtractionManagerTest,
       ShouldNotTriggerWhenSuggestionIsSuppressed) {
  EXPECT_THAT(
      amount_extraction_manager_->GetEligibleFeatures(
          /*is_autofill_payments_enabled=*/true,
          /*should_suppress_suggestions=*/true,
          /*suggestions=*/
          std::vector<Suggestion>{Suggestion(SuggestionType::kCreditCardEntry)},
          /*filling_product=*/FillingProduct::kCreditCard,
          /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
      IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerWhenNoSuggestion) {
  EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                  /*is_autofill_payments_enabled=*/true,
                  /*should_suppress_suggestions=*/false,
                  /*suggestions=*/{},
                  /*filling_product=*/FillingProduct::kCreditCard,
                  /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
              IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerIfUrlNotEligible) {
  ON_CALL(
      *static_cast<MockAutofillOptimizationGuideDecider*>(
          autofill_manager().client().GetAutofillOptimizationGuideDecider()),
      IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(false));

  EXPECT_THAT(
      amount_extraction_manager_->GetEligibleFeatures(
          /*is_autofill_payments_enabled=*/true,
          /*should_suppress_suggestions=*/false,
          /*suggestions=*/
          std::vector<Suggestion>{Suggestion(SuggestionType::kCreditCardEntry)},
          /*filling_product=*/FillingProduct::kCreditCard,
          /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
      IsEmpty());
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerInIncognitoMode) {
  std::vector<FieldType> field_types = {FieldType::CREDIT_CARD_NUMBER,
                                        FieldType::CREDIT_CARD_NAME_FULL,
                                        FieldType::CREDIT_CARD_EXP_MONTH};
  autofill_client().set_is_off_the_record(/*is_off_the_record=*/true);

  for (FieldType field_type : field_types) {
    EXPECT_THAT(amount_extraction_manager_->GetEligibleFeatures(
                    /*is_autofill_payments_enabled=*/true,
                    /*should_suppress_suggestions=*/false,
                    /*suggestions=*/
                    std::vector<Suggestion>{
                        Suggestion(SuggestionType::kCreditCardEntry)},
                    /*filling_product=*/FillingProduct::kCreditCard,
                    /*field_type=*/field_type),
                IsEmpty());
  }
}

TEST_F(AmountExtractionManagerTest, ShouldNotTriggerIfNoBnplIssuer) {
  payments_data().ClearBnplIssuers();

  EXPECT_THAT(
      amount_extraction_manager_->GetEligibleFeatures(
          /*is_autofill_payments_enabled=*/true,
          /*should_suppress_suggestions=*/false,
          /*suggestions=*/
          std::vector<Suggestion>{Suggestion(SuggestionType::kCreditCardEntry)},
          /*filling_product=*/FillingProduct::kCreditCard,
          /*field_type=*/FieldType::CREDIT_CARD_NUMBER),
      IsEmpty());
}

// This test checks when the search is triggered,
// `ExtractLabeledTextNodeValue` from `AutofillDriver` is invoked.
TEST_F(AmountExtractionManagerTest, TriggerCheckoutAmountExtraction) {
  EXPECT_CALL(
      autofill_driver(),
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

TEST_F(AmountExtractionManagerTest, ValidResponse) {
  optimization_guide::proto::AmountExtractionResponse response;
  response.set_final_checkout_amount(123.45);
  response.set_currency("USD");
  response.set_is_successful(true);

  EXPECT_TRUE(
      amount_extraction_manager_->IsValidAmountExtractionResponse(response));
}

TEST_F(AmountExtractionManagerTest, ValidResponseZeroAmount) {
  optimization_guide::proto::AmountExtractionResponse response;
  response.set_final_checkout_amount(0.0);
  response.set_currency("EUR");
  EXPECT_TRUE(
      amount_extraction_manager_->IsValidAmountExtractionResponse(response));
}

TEST_F(AmountExtractionManagerTest, MissingAmount) {
  optimization_guide::proto::AmountExtractionResponse response;
  response.set_currency("USD");
  EXPECT_FALSE(
      amount_extraction_manager_->IsValidAmountExtractionResponse(response));
}

TEST_F(AmountExtractionManagerTest, NegativeAmount) {
  optimization_guide::proto::AmountExtractionResponse response;
  response.set_final_checkout_amount(-10.50);
  response.set_currency("USD");
  EXPECT_FALSE(
      amount_extraction_manager_->IsValidAmountExtractionResponse(response));
}

TEST_F(AmountExtractionManagerTest, MissingCurrency) {
  optimization_guide::proto::AmountExtractionResponse response;
  response.set_final_checkout_amount(123.45);
  EXPECT_FALSE(
      amount_extraction_manager_->IsValidAmountExtractionResponse(response));
}

TEST_F(AmountExtractionManagerTest, NonAsciiCurrency) {
  optimization_guide::proto::AmountExtractionResponse response;
  response.set_final_checkout_amount(123.45);
  response.set_currency("USĐ");
  EXPECT_FALSE(
      amount_extraction_manager_->IsValidAmountExtractionResponse(response));
}

TEST_F(AmountExtractionManagerTest, CurrencyTooShort) {
  optimization_guide::proto::AmountExtractionResponse response;
  response.set_final_checkout_amount(123.45);
  response.set_currency("US");
  EXPECT_FALSE(
      amount_extraction_manager_->IsValidAmountExtractionResponse(response));
}

TEST_F(AmountExtractionManagerTest, CurrencyTooLong) {
  optimization_guide::proto::AmountExtractionResponse response;
  response.set_final_checkout_amount(123.45);
  response.set_currency("USDD");
  EXPECT_FALSE(
      amount_extraction_manager_->IsValidAmountExtractionResponse(response));
}

TEST_F(AmountExtractionManagerTest, CurrencyWithLowerCase) {
  optimization_guide::proto::AmountExtractionResponse response;
  response.set_final_checkout_amount(123.45);
  response.set_currency("UsD");
  EXPECT_FALSE(
      amount_extraction_manager_->IsValidAmountExtractionResponse(response));
}

TEST_F(AmountExtractionManagerTest, EmptyResponse) {
  optimization_guide::proto::AmountExtractionResponse response;
  EXPECT_FALSE(
      amount_extraction_manager_->IsValidAmountExtractionResponse(response));
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
      autofill_driver(),
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
      "Autofill.AmountExtraction.Latency2.Success",
      base::Milliseconds(kDefaultAmountExtractionLatencyMs), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.AmountExtraction.Latency2",
      base::Milliseconds(kDefaultAmountExtractionLatencyMs), 1);

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::Autofill_AmountExtractionComplete::kEntryName,
      {ukm::builders::Autofill_AmountExtractionComplete::
           kFailureLatencyInMillisName,
       ukm::builders::Autofill_AmountExtractionComplete::
           kSuccessLatencyInMillisName,
       ukm::builders::Autofill_AmountExtractionComplete::kResultName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("SuccessLatencyInMillis"),
            kDefaultAmountExtractionLatencyMs);
  EXPECT_EQ(
      ukm_entries[0].metrics.at("Result"),
      static_cast<uint8_t>(
          autofill::autofill_metrics::AmountExtractionResult::kSuccessful));
}

TEST_F(AmountExtractionManagerTest,
       TriggerCheckoutAmountExtraction_Failure_Metric) {
  constexpr int kDefaultAmountExtractionLatencyMs = 200;
  base::HistogramTester histogram_tester;
  SetUpCheckoutAmountExtractionCall(
      /*extracted_amount=*/"",
      /*latency_ms=*/kDefaultAmountExtractionLatencyMs);
  EXPECT_CALL(
      autofill_driver(),
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
      "Autofill.AmountExtraction.Latency2.Failure",
      base::Milliseconds(kDefaultAmountExtractionLatencyMs), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.AmountExtraction.Latency2",
      base::Milliseconds(kDefaultAmountExtractionLatencyMs), 1);

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::Autofill_AmountExtractionComplete::kEntryName,
      {ukm::builders::Autofill_AmountExtractionComplete::
           kFailureLatencyInMillisName,
       ukm::builders::Autofill_AmountExtractionComplete::
           kSuccessLatencyInMillisName,
       ukm::builders::Autofill_AmountExtractionComplete::kResultName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("FailureLatencyInMillis"),
            kDefaultAmountExtractionLatencyMs);
  EXPECT_EQ(
      ukm_entries[0].metrics.at("Result"),
      static_cast<uint8_t>(
          autofill::autofill_metrics::AmountExtractionResult::kAmountNotFound));
}

// Verify that Amount extraction records true for a successful extraction.
TEST_F(AmountExtractionManagerTest, AmountExtractionResult_Metric_Successful) {
  constexpr std::string kExtractedAmount = "123.45";
  base::HistogramTester histogram_tester;
  SetUpCheckoutAmountExtractionCall(
      /*extracted_amount=*/kExtractedAmount,
      /*latency_ms=*/0);
  EXPECT_CALL(
      autofill_driver(),
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
      "Autofill.AmountExtraction.Result2",
      autofill::autofill_metrics::AmountExtractionResult::kSuccessful, 1);

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::Autofill_AmountExtractionComplete::kEntryName,
      {ukm::builders::Autofill_AmountExtractionComplete::kResultName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(
      ukm_entries[0].metrics.at("Result"),
      static_cast<uint8_t>(
          autofill::autofill_metrics::AmountExtractionResult::kSuccessful));
}

// Verify that Amount extraction records false for a failed extraction.
TEST_F(AmountExtractionManagerTest,
       AmountExtractionResult_Metric_AmountNotFound) {
  base::HistogramTester histogram_tester;
  SetUpCheckoutAmountExtractionCall(
      /*extracted_amount=*/"",
      /*latency_ms=*/0);
  EXPECT_CALL(
      autofill_driver(),
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
      "Autofill.AmountExtraction.Result2",
      autofill::autofill_metrics::AmountExtractionResult::kAmountNotFound, 1);

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::Autofill_AmountExtractionComplete::kEntryName,
      {ukm::builders::Autofill_AmountExtractionComplete::kResultName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(
      ukm_entries[0].metrics.at("Result"),
      static_cast<uint8_t>(
          autofill::autofill_metrics::AmountExtractionResult::kAmountNotFound));
}

TEST_F(AmountExtractionManagerTest, AmountExtractionResult_Metric_Timeout) {
  base::HistogramTester histogram_tester;
  ON_CALL(autofill_driver(), ExtractLabeledTextNodeValue)
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
      "Autofill.AmountExtraction.Result2",
      autofill::autofill_metrics::AmountExtractionResult::kTimeout, 1);

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::Autofill_AmountExtractionComplete::kEntryName,
      {ukm::builders::Autofill_AmountExtractionComplete::kResultName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Result"),
            static_cast<uint8_t>(
                autofill::autofill_metrics::AmountExtractionResult::kTimeout));
}

TEST_F(AmountExtractionManagerTest, TimeoutExpiresBeforeResponse) {
  mock_amount_extraction_manager_ =
      std::make_unique<MockAmountExtractionManager>(&autofill_manager());
  EXPECT_FALSE(
      test_api(*mock_amount_extraction_manager_).GetSearchRequestPending());
  EXPECT_CALL(autofill_driver(), ExtractLabeledTextNodeValue)
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
      std::make_unique<MockAmountExtractionManager>(&autofill_manager());
  EXPECT_FALSE(
      test_api(*mock_amount_extraction_manager_).GetSearchRequestPending());
  EXPECT_CALL(autofill_driver(), ExtractLabeledTextNodeValue)
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
  EXPECT_CALL(*autofill_manager().GetPaymentsBnplManager(),
              OnAmountExtractionReturned(std::optional<int64_t>(),
                                         /*timeout_reached=*/false))
      .Times(1);

  FakeCheckoutAmountReceived("");
}

// This test checks that the BNPL manager will be notified when the amount
// extraction receives a result with correct format.
TEST_F(AmountExtractionManagerTest,
       OnCheckoutAmountReceived_AmountInCorrectFormat_BnplManagerNotified) {
  EXPECT_CALL(*autofill_manager().GetPaymentsBnplManager(),
              OnAmountExtractionReturned(std::optional<int64_t>(123'450'000LL),
                                         /*timeout_reached=*/false))
      .Times(1);

  FakeCheckoutAmountReceived("$ 123.45");
}

// This test checks that the BNPL manager will be notified when the amount
// extraction times out.
TEST_F(AmountExtractionManagerTest,
       OnCheckoutAmountReceived_AmountExtractionTimeout_BnplManagerNotified) {
  EXPECT_CALL(
      *autofill_manager().GetPaymentsBnplManager(),
      OnAmountExtractionReturned(Eq(std::nullopt), /*timeout_reached=*/true))
      .Times(1);

  FakeAmountExtractionTimeout();
}

// This test checks that `BnplManager::OnAmountExtractionReturned` will not be
// invoked when an invalid amount extraction result is received from the
// server-side AI.
TEST_F(AmountExtractionManagerTest,
       OnCheckoutAmountReceivedFromAi_InvalidResult) {
  EXPECT_CALL(*autofill_manager().GetPaymentsBnplManager(),
              OnAmountExtractionReturnedFromAi(std::optional<int64_t>(), false))
      .Times(1);

  FakeCheckoutAmountReceivedFromAi(123.45, "InvalidCurrency", true);
}

// This test checks that `BnplManager::OnAmountExtractionReturned` will be
// invoked when a valid amount extraction result is received from the
// server-side AI.
TEST_F(AmountExtractionManagerTest,
       OnCheckoutAmountReceivedFromAi_ValidResult) {
  EXPECT_CALL(*autofill_manager().GetPaymentsBnplManager(),
              OnAmountExtractionReturnedFromAi(
                  std::optional<int64_t>(123'450'000LL), false))
      .Times(1);

  FakeCheckoutAmountReceivedFromAi(123.45, "USD", true);
}

// This test checks AutofillClient::GetAiPageContent is called when no page
// content is present and not currently fetching.
TEST_F(AmountExtractionManagerTest, FetchAiPageContent_StartsFetching) {
  EXPECT_FALSE(
      test_api(*amount_extraction_manager_).GetIsFetchingAiPageContent());
  EXPECT_EQ(test_api(*amount_extraction_manager_).GetAiPageContent(), nullptr);

  EXPECT_CALL(autofill_client(), GetAiPageContent);
  amount_extraction_manager_->FetchAiPageContent();

  EXPECT_TRUE(
      test_api(*amount_extraction_manager_).GetIsFetchingAiPageContent());
}

// This test checks AutofillClient::GetAiPageContent is not called when a fetch
// is already in progress.
TEST_F(AmountExtractionManagerTest, FetchAiPageContent_AlreadyFetching) {
  test_api(*amount_extraction_manager_).SetIsFetchingAiPageContent(true);

  EXPECT_CALL(autofill_client(), GetAiPageContent).Times(0);
  amount_extraction_manager_->FetchAiPageContent();

  EXPECT_TRUE(
      test_api(*amount_extraction_manager_).GetIsFetchingAiPageContent());
}

// This test checks AutofillClient::GetAiPageContent is called and the callback
// receives a valid value.
TEST_F(AmountExtractionManagerTest, OnAiPageContentReceived_ValidValue) {
  optimization_guide::proto::AnnotatedPageContent test_proto;
  test_proto.set_tab_id(1234);  // Example data
  EXPECT_FALSE(
      test_api(*amount_extraction_manager_).GetIsFetchingAiPageContent());

  EXPECT_CALL(autofill_client(), GetAiPageContent)
      .WillOnce([&](base::OnceCallback<void(
                        std::optional<
                            optimization_guide::proto::AnnotatedPageContent>)>
                        callback) {
        EXPECT_TRUE(
            test_api(*amount_extraction_manager_).GetIsFetchingAiPageContent());
        std::move(callback).Run(std::make_optional(test_proto));
      });

  amount_extraction_manager_->FetchAiPageContent();

  EXPECT_FALSE(
      test_api(*amount_extraction_manager_).GetIsFetchingAiPageContent());
  EXPECT_NE(test_api(*amount_extraction_manager_).GetAiPageContent(), nullptr);
  EXPECT_THAT(
      test_api(*amount_extraction_manager_).GetAiPageContent()->tab_id(),
      Eq(1234));
}

// This test checks AutofillClient::GetAiPageContent is called and the callback
// receives nullptr.
TEST_F(AmountExtractionManagerTest, OnAiPageContentReceived_NullOpt) {
  EXPECT_FALSE(
      test_api(*amount_extraction_manager_).GetIsFetchingAiPageContent());

  EXPECT_CALL(autofill_client(), GetAiPageContent)
      .WillOnce([&](base::OnceCallback<void(
                        std::optional<
                            optimization_guide::proto::AnnotatedPageContent>)>
                        callback) {
        EXPECT_TRUE(
            test_api(*amount_extraction_manager_).GetIsFetchingAiPageContent());
        std::move(callback).Run(std::nullopt);
      });

  amount_extraction_manager_->FetchAiPageContent();

  EXPECT_FALSE(
      test_api(*amount_extraction_manager_).GetIsFetchingAiPageContent());
  EXPECT_THAT(test_api(*amount_extraction_manager_).GetAiPageContent(),
              nullptr);
}

// Verify that if the page content is not fetched, `ExecuteModel` should not be
// called.
TEST_F(AmountExtractionManagerTest, ShouldNotCallExecuteModel) {
  ASSERT_EQ(test_api(*amount_extraction_manager_).GetAiPageContent(), nullptr);
  EXPECT_CALL(*model_executor(), ExecuteModel).Times(0);
  amount_extraction_manager_->TriggerCheckoutAmountExtractionWithAi();
}

// Verify that when `TriggerCheckoutAmountExtractionWithAi` is called with
// annotated page contents present, `ExecuteModel` from
// `RemoteModelExecutor` should be invoked.
TEST_F(AmountExtractionManagerTest, ShouldCallExecuteModel) {
  test_api(*amount_extraction_manager_).SetAiPageContent();
  optimization_guide::proto::AmountExtractionRequest expected_request;
  *expected_request.mutable_annotated_page_content() =
      optimization_guide::proto::AnnotatedPageContent();

  optimization_guide::ModelExecutionOptions expected_options{
      .execution_timeout =
          AmountExtractionManager::kAiBasedAmountExtractionWaitTime};

  optimization_guide::proto::AmountExtractionResponse response;
  response.set_final_checkout_amount(123.45);
  response.set_currency("USD");
  response.set_is_successful(true);

  EXPECT_CALL(
      *model_executor(),
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kAmountExtraction,
          EqualsProto(expected_request), Eq(expected_options),
          A<ModelExecutionCallback>()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideModelExecutionResult(
              optimization_guide::AnyWrapProto(response),
              /*execution_info=*/nullptr),
          /*log_entry=*/nullptr));

  amount_extraction_manager_->TriggerCheckoutAmountExtractionWithAi();

  ASSERT_EQ(test_api(*amount_extraction_manager_).GetAiPageContent(), nullptr);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace autofill::payments
