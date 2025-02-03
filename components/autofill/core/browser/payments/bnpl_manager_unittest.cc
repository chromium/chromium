// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_manager.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/bnpl_manager_test_api.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

using testing::_;
using testing::FieldsAre;
using testing::Test;

namespace {
class PaymentsNetworkInterfaceMock : public PaymentsNetworkInterface {
 public:
  PaymentsNetworkInterfaceMock()
      : PaymentsNetworkInterface(nullptr, nullptr, nullptr) {}

  MOCK_METHOD(
      void,
      GetBnplPaymentInstrumentForFetchingVcn,
      (GetBnplPaymentInstrumentForFetchingVcnRequestDetails request_details,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                               const BnplFetchVcnResponseDetails&)> callback));
};
}  // namespace

class BnplManagerTest : public Test {
 public:
  const int64_t kBillingCustomerNumber = 1234;
  const std::string kRiskData = "RISK_DATA";
  const std::string kInstrumentId = "INSTRUMENT_ID";
  const std::string kContextToken = "CONTEXT_TOKEN";
  const GURL kRedirectUrl = GURL("REDIRECT_URL");
  const std::string kIssuerId = "ISSUER_ID";

  void SetUp() override {
    autofill_client_ = std::make_unique<TestAutofillClient>();
    autofill_client_->SetPrefs(test::PrefServiceForTesting());
    autofill_client_->SetAutofillPaymentMethodsEnabled(true);
    autofill_client_->GetPersonalDataManager()
        .payments_data_manager()
        .SetSyncingForTest(true);
    autofill_client_->GetPersonalDataManager().SetPrefService(
        autofill_client_->GetPrefs());

    std::unique_ptr<PaymentsNetworkInterfaceMock> payments_network_interface =
        std::make_unique<PaymentsNetworkInterfaceMock>();
    payments_network_interface_ = payments_network_interface.get();

    autofill_client_->GetPaymentsAutofillClient()
        ->set_payments_network_interface(std::move(payments_network_interface));

    bnpl_manager_ = std::make_unique<BnplManager>(
        autofill_client_->GetPaymentsAutofillClient());
  }

  // Sets up the PersonalDataManager with a unlinked bnpl issuer.
  void SetUpUnlinkedBnplIssuer(uint64_t price_lower_bound,
                               uint64_t price_higher_bound,
                               const std::string& issuer_id) {
    std::vector<BnplIssuer::EligiblePriceRange> eligible_price_ranges;
    eligible_price_ranges.emplace_back(/*currency=*/"USD",
                                       price_lower_bound * kMicrosPerDollar,
                                       price_higher_bound * kMicrosPerDollar);
    test_api(autofill_client_->GetPersonalDataManager().payments_data_manager())
        .AddBnplIssuer(BnplIssuer(std::nullopt, issuer_id,
                                  std::move(eligible_price_ranges)));
  }

  // Sets up the PersonalDataManager with a linked bnpl issuer.
  void SetUpLinkedBnplIssuer(uint64_t price_lower_bound,
                             uint64_t price_higher_bound,
                             const std::string& issuer_id,
                             const int64_t instrument_id) {
    std::vector<BnplIssuer::EligiblePriceRange> eligible_price_ranges;
    eligible_price_ranges.emplace_back(/*currency=*/"USD",
                                       price_lower_bound * kMicrosPerDollar,
                                       price_higher_bound * kMicrosPerDollar);

    test_api(autofill_client_->GetPersonalDataManager().payments_data_manager())
        .AddBnplIssuer(BnplIssuer(instrument_id, issuer_id,
                                  std::move(eligible_price_ranges)));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  std::unique_ptr<BnplManager> bnpl_manager_;
  raw_ptr<PaymentsNetworkInterfaceMock> payments_network_interface_;
};

// BNPL is currently only available for desktop platforms.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to monetary values they represent in micro-units when given empty
// string or zeros.
TEST_F(BnplManagerTest, AmountParser_Zeros) {
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits(""),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$0"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$0.00"), 0ULL);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to monetary values they represent in micro-units when given normal
// format of strings.
TEST_F(BnplManagerTest, AmountParser_NormalCases) {
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 12.34"),
            12'340'000ULL);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 012.34"),
            12'340'000ULL);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("USD 1,234.56"),
            1'234'560'000ULL);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 1,234.56"),
            1'234'560'000ULL);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 123.45"),
            123'450'000ULL);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$0.12"),
            120'000ULL);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("USD   0.12"),
            120'000ULL);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to monetary values they represent in micro-units when given input
// string with leading and tailing monetary-representing substrings.
TEST_F(BnplManagerTest, AmountParser_LeadingAndTailingCharacters) {
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$   1,234.56   USD"),
      1'234'560'000ULL);
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("USD $ 1,234.56 USD"),
      1'234'560'000ULL);
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("  $ 1,234.56 "),
      1'234'560'000ULL);
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("USD    1234.56    "),
      1'234'560'000ULL);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to std::nullopt when given negative value strings.
TEST_F(BnplManagerTest, AmountParser_NegativeValue) {
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ -1,234.56"),
            std::nullopt);
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("USD -1,234.56"),
      std::nullopt);
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("USD 1,234.56- $"),
      std::nullopt);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to std::nullopt when given incorrect format of strings.
TEST_F(BnplManagerTest, AmountParser_IncorrectFormatOfInputs) {
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ ,123.45"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$1,234.5"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("NaN"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("Inf"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("-Inf"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("1.234E8"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$1.234.56"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 12e2"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 12e2.23"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$ 12.23e2"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("E1.23"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("E1.23"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("e1.23"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("-1.23"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("1.23E"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("1.23e"),
            std::nullopt);
  EXPECT_EQ(bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("1.23-"),
            std::nullopt);
}

// Tests that the MaybeParseAmountToMonetaryMicroUnits parser converts the input
// strings to std::nullopt when the converted value overflows uint64.
TEST_F(BnplManagerTest, AmountParser_OverflowValue) {
  EXPECT_EQ(
      bnpl_manager_->MaybeParseAmountToMonetaryMicroUnits("$19000000000000.00"),
      std::nullopt);
}

// Tests that the initial state for a BNPL flow is set when
// BnplManager::InitBnplFlow() is triggered.
TEST_F(BnplManagerTest, InitBnplFlow_SetsInitialState) {
  uint64_t final_checkout_amount = 1000000;
  bnpl_manager_->InitBnplFlow(final_checkout_amount, base::DoNothing());

  EXPECT_EQ(
      final_checkout_amount,
      test_api(*bnpl_manager_).GetOngoingFlowState()->final_checkout_amount);
  EXPECT_FALSE(test_api(*bnpl_manager_)
                   .GetOngoingFlowState()
                   ->on_bnpl_vcn_fetched_callback.is_null());
}

// Tests that FetchVcnDetails calls the payments network interface with the
// request details filled out correctly, and once the VCN is filled the state of
// BnplManager is reset.
TEST_F(BnplManagerTest, FetchVcnDetails_CallsGetBnplPaymentInstrument) {
  bnpl_manager_->InitBnplFlow(1000000, base::DoNothing());
  test_api(*bnpl_manager_)
      .PopulateManagerWithUserAndBnplIssuerDetails(
          kBillingCustomerNumber, kInstrumentId, kRiskData, kContextToken,
          kRedirectUrl, kIssuerId);

  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingVcn(
                  /*request_details=*/
                  FieldsAre(kBillingCustomerNumber, kInstrumentId, kRiskData,
                            kContextToken, kRedirectUrl, kIssuerId),
                  /*callback=*/_));

  EXPECT_NE(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);

  test_api(*bnpl_manager_).FetchVcnDetails();
  test_api(*bnpl_manager_)
      .OnVcnDetailsFetched(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                           BnplFetchVcnResponseDetails());

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that update suggestions callback is called when suggestions are shown
// before amount extraction completion.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_SuggestionShownFirstThenAmountExtractionReturned) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterForAffirm,
                            features::kAutofillEnableBuyNowPayLaterForZip},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  base::MockCallback<UpdateSuggestionsCallback> callback;
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  EXPECT_CALL(callback, Run).Times(1);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
  bnpl_manager_->OnAmountExtractionReturned("$1,234.56");
}

// Tests that update suggestions callback is called when suggestions are shown
// after amount extraction completion.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_AmountExtractionReturnedFirstThenSuggestionShown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterForAffirm,
                            features::kAutofillEnableBuyNowPayLaterForZip},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  base::MockCallback<UpdateSuggestionsCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnAmountExtractionReturned("$1,234.56");
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
}

// Tests that update suggestions callback will not be called if the extracted
// amount is not supported by available BNPL issuers.
TEST_F(BnplManagerTest, AddBnplSuggestion_AmountNotSupported) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterForAffirm,
                            features::kAutofillEnableBuyNowPayLaterForZip},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  base::MockCallback<UpdateSuggestionsCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnAmountExtractionReturned("$30.00");
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
}

// Tests that update suggestions callback will not be called if the BNPL
// features are disabled.
TEST_F(BnplManagerTest, AddBnplSuggestion_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLaterForAffirm,
                             features::kAutofillEnableBuyNowPayLaterForZip});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  base::MockCallback<UpdateSuggestionsCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnAmountExtractionReturned("$1,234.56");
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
}

// Tests that update suggestions callback will not be called if the extracted
// amount is only supported by Affirm, but the feature flag for Affirm is not
// enabled.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_AffirmDisabledZipEnabled_AmountSupportedByAffirm) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterForZip},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLaterForAffirm});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  base::MockCallback<UpdateSuggestionsCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnAmountExtractionReturned("$50.00");
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
}

// Tests that update suggestions callback will be called if the extracted
// amount is only supported by Affirm, and the feature flag for Affirm is
// enabled.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_AffirmEnabledZipDisabled_AmountSupportedByAffirm) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterForAffirm},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLaterForZip});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  base::MockCallback<UpdateSuggestionsCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnAmountExtractionReturned("$50.00");
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
}

// Tests that update suggestions callback will not be called if the extracted
// amount is only supported by Zip, but the feature flag for Zip is not enabled.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_ZipDisabledAffirmEnabled_AmountSupportedByZip) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterForAffirm},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLaterForZip});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  base::MockCallback<UpdateSuggestionsCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnAmountExtractionReturned("$1,234.56");
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
}

// Tests that update suggestions callback will be called if the extracted
// amount is only supported by Zip, and the feature flag for Zip is enabled.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_ZipEnabledAffirmDisabled_AmountSupportedByZip) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterForZip},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLaterForAffirm});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  base::MockCallback<UpdateSuggestionsCallback> callback;
  EXPECT_CALL(callback, Run).Times(1);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnAmountExtractionReturned("$1,234.56");
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace autofill::payments
