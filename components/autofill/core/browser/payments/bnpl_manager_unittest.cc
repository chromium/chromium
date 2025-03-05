// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_manager.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/bnpl_manager_test_api.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
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
  MOCK_METHOD(
      void,
      CreateBnplPaymentInstrument,
      (const CreateBnplPaymentInstrumentRequestDetails& request_details,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                               std::u16string instrument_id)> callback));
  MOCK_METHOD(
      void,
      GetDetailsForCreateBnplPaymentInstrument,
      (const GetDetailsForCreateBnplPaymentInstrumentRequestDetails&,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                               std::string context_token,
                               std::unique_ptr<base::Value::Dict>)>));
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
  const std::string kAppLocale = "en-GB";
  const std::u16string kLegalMessage = u"LEGAL_MESSAGE";

  void SetUp() override {
    autofill_client_ = std::make_unique<TestAutofillClient>();
    autofill_client_->SetPrefs(test::PrefServiceForTesting());
    autofill_client_->set_app_locale(kAppLocale);
    autofill_client_->SetAutofillPaymentMethodsEnabled(true);
    autofill_client_->GetPersonalDataManager()
        .payments_data_manager()
        .SetSyncingForTest(true);
    autofill_client_->GetPersonalDataManager()
        .test_payments_data_manager()
        .SetPaymentsCustomerData(std::make_unique<PaymentsCustomerData>(
            base::NumberToString(kBillingCustomerNumber)));
    autofill_client_->GetPersonalDataManager().SetPrefService(
        autofill_client_->GetPrefs());

    std::unique_ptr<PaymentsNetworkInterfaceMock> payments_network_interface =
        std::make_unique<PaymentsNetworkInterfaceMock>();
    payments_network_interface_ = payments_network_interface.get();

    autofill_client_->GetPaymentsAutofillClient()
        ->set_payments_network_interface(std::move(payments_network_interface));

    bnpl_manager_ = std::make_unique<BnplManager>(autofill_client_.get());
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

  void TriggerBnplUpdateSuggestionsFlow(
      bool expect_suggestions_are_updated,
      std::optional<uint64_t> extracted_amount) {
    std::vector<Suggestion> suggestions = {
        Suggestion(SuggestionType::kCreditCardEntry),
        Suggestion(SuggestionType::kManageCreditCard)};
    base::MockCallback<UpdateSuggestionsCallback> callback;
    expect_suggestions_are_updated ? EXPECT_CALL(callback, Run).Times(1)
                                   : EXPECT_CALL(callback, Run).Times(0);

    bnpl_manager_->NotifyOfSuggestionGeneration(
        AutofillSuggestionTriggerSource::kUnspecified);
    bnpl_manager_->OnAmountExtractionReturned(extracted_amount);
    bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
  }

  void OnIssuerSelected(const BnplIssuer& selected_issuer) {
    bnpl_manager_->OnIssuerSelected(selected_issuer);
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
// Tests that the initial state for a BNPL flow is set when
// BnplManager::InitBnplFlow() is triggered.
TEST_F(BnplManagerTest, InitBnplFlow_SetsInitialState) {
  uint64_t final_checkout_amount = 1'000'000;
  bnpl_manager_->InitBnplFlow(final_checkout_amount, base::DoNothing());

  EXPECT_EQ(
      final_checkout_amount,
      test_api(*bnpl_manager_).GetOngoingFlowState()->final_checkout_amount);
  EXPECT_EQ(autofill_client_->GetAppLocale(),
            test_api(*bnpl_manager_).GetOngoingFlowState()->app_locale);
  EXPECT_EQ(
      GetBillingCustomerId(autofill_client_->GetPaymentsAutofillClient()
                               ->GetPaymentsDataManager()),
      test_api(*bnpl_manager_).GetOngoingFlowState()->billing_customer_number);
  EXPECT_FALSE(test_api(*bnpl_manager_)
                   .GetOngoingFlowState()
                   ->on_bnpl_vcn_fetched_callback.is_null());
  EXPECT_FALSE(
      test_api(*bnpl_manager_).GetOngoingFlowState()->risk_data.empty());
}

// Tests that the initial state for a BNPL flow is set when
// BnplManager::InitBnplFlow() is triggered, even if the app locale is not
// "en-US". This helps test that the flow is easily scalable to other app
// locales.
TEST_F(BnplManagerTest, InitBnplFlow_SetsInitialStateWithDifferentAppLocale) {
  uint64_t final_checkout_amount = 1000000;
  autofill_client_->set_app_locale("en_GB");
  bnpl_manager_->InitBnplFlow(final_checkout_amount, base::DoNothing());

  EXPECT_EQ(
      final_checkout_amount,
      test_api(*bnpl_manager_).GetOngoingFlowState()->final_checkout_amount);
  EXPECT_EQ(autofill_client_->GetAppLocale(),
            test_api(*bnpl_manager_).GetOngoingFlowState()->app_locale);
  EXPECT_EQ(
      GetBillingCustomerId(autofill_client_->GetPaymentsAutofillClient()
                               ->GetPaymentsDataManager()),
      test_api(*bnpl_manager_).GetOngoingFlowState()->billing_customer_number);
  EXPECT_FALSE(test_api(*bnpl_manager_)
                   .GetOngoingFlowState()
                   ->on_bnpl_vcn_fetched_callback.is_null());
  EXPECT_FALSE(
      test_api(*bnpl_manager_).GetOngoingFlowState()->risk_data.empty());
}

// Tests that the the user accepting the ToS dialog triggers a
// CreatePaymentInstrument request and loads risk data after ToS dialog
// acceptance if it was not already loaded.
TEST_F(BnplManagerTest, TosDialogAccepted_PrefetchedRiskDataNotLoaded) {
  bnpl_manager_->InitBnplFlow(/*final_checkout_amount=*/1000000,
                              base::DoNothing());
  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  std::string test_context_token = "test_context_token";
  std::string test_issuer_id = std::string(kBnplAffirmIssuerId);
  ongoing_flow_state->context_token = test_context_token;
  ongoing_flow_state->issuer_id = test_issuer_id;
  ongoing_flow_state->risk_data.clear();

  ASSERT_TRUE(ongoing_flow_state->risk_data.empty());

  EXPECT_CALL(
      *payments_network_interface_,
      CreateBnplPaymentInstrument(/*request_details=*/
                                  FieldsAre(
                                      autofill_client_->GetAppLocale(),
                                      GetBillingCustomerId(
                                          autofill_client_
                                              ->GetPaymentsAutofillClient()
                                              ->GetPaymentsDataManager()),
                                      test_issuer_id, test_context_token,
                                      /*risk_data=*/_),
                                  /*callback=*/_));
  test_api(*bnpl_manager_).OnTosDialogAccepted();

  EXPECT_FALSE(ongoing_flow_state->risk_data.empty());
}

// Tests that the the user accepting the ToS dialog triggers a
// CreatePaymentInstrument request with the loaded risk data, if it is present.
TEST_F(BnplManagerTest, TosDialogAccepted_PrefetchedRiskDataLoaded) {
  bnpl_manager_->InitBnplFlow(/*final_checkout_amount=*/1000000,
                              base::DoNothing());
  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  std::string test_context_token = "test_context_token";
  std::string test_issuer_id = std::string(kBnplAffirmIssuerId);
  std::string risk_data = ongoing_flow_state->risk_data;
  ongoing_flow_state->context_token = test_context_token;
  ongoing_flow_state->issuer_id = test_issuer_id;

  ASSERT_FALSE(ongoing_flow_state->risk_data.empty());

  autofill_client_->GetPaymentsAutofillClient()->set_risk_data_loaded(false);

  EXPECT_CALL(
      *payments_network_interface_,
      CreateBnplPaymentInstrument(/*request_details=*/
                                  FieldsAre(
                                      autofill_client_->GetAppLocale(),
                                      GetBillingCustomerId(
                                          autofill_client_
                                              ->GetPaymentsAutofillClient()
                                              ->GetPaymentsDataManager()),
                                      test_issuer_id, test_context_token,
                                      risk_data),
                                  /*callback=*/_));
  test_api(*bnpl_manager_).OnTosDialogAccepted();

  EXPECT_FALSE(ongoing_flow_state->risk_data.empty());

  // Since risk data was cached, it was directly used, thus loading risk data
  // was skipped.
  EXPECT_FALSE(
      autofill_client_->GetPaymentsAutofillClient()->risk_data_loaded());
}

// Tests that FetchVcnDetails calls the payments network interface with the
// request details filled out correctly, and once the VCN is filled the state of
// BnplManager is reset.
TEST_F(BnplManagerTest, FetchVcnDetails_CallsGetBnplPaymentInstrument) {
  bnpl_manager_->InitBnplFlow(1'000'000, base::DoNothing());
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

// Tests that `OnIssuerSelected()` calls with an unlinked BNPL issuer will call
// the payments network interface with the request details filled out correctly.
TEST_F(
    BnplManagerTest,
    OnIssuerSelected_CallsGetDetailsForCreateBnplPaymentInstrument_UnlinkedIssuer) {
  bnpl_manager_->InitBnplFlow(1'000'000, base::DoNothing());

  ASSERT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->app_locale,
            kAppLocale);
  ASSERT_EQ(
      test_api(*bnpl_manager_).GetOngoingFlowState()->billing_customer_number,
      kBillingCustomerNumber);

  BnplIssuer unlinked_issuer = test::GetTestUnlinkedBnplIssuer();

  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForCreateBnplPaymentInstrument(
                  /*request_details=*/
                  FieldsAre(kAppLocale, kBillingCustomerNumber,
                            unlinked_issuer.issuer_id()),
                  /*callback=*/_))
      .Times(1);

  OnIssuerSelected(unlinked_issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->issuer_id,
            unlinked_issuer.issuer_id());
}

// Tests that `OnDidGetDetailsForCreateBnplPaymentInstrument` set the BNPL
// manager state if the request has completed successfully.
TEST_F(BnplManagerTest, OnDidGetDetailsForCreateBnplPaymentInstrument) {
  bnpl_manager_->InitBnplFlow(1'000'000, base::DoNothing());
  BnplIssuer unlinked_issuer = test::GetTestUnlinkedBnplIssuer();

  // Set up legal message for testing.
  auto legal_message = std::make_unique<base::Value::Dict>();
  legal_message->Set("line",
                     base::Value::List().Append(base::Value::Dict().Set(
                         "template", base::UTF16ToUTF8(kLegalMessage))));

  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForCreateBnplPaymentInstrument)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, kContextToken,
          std::move(legal_message)));
  OnIssuerSelected(unlinked_issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->context_token,
            kContextToken);

  LegalMessageLines& legal_message_lines =
      test_api(*bnpl_manager_).GetOngoingFlowState()->legal_message_lines;
  ASSERT_FALSE(legal_message_lines.empty());
  EXPECT_EQ(legal_message_lines[0].text(), kLegalMessage);
}

// Tests that `OnDidGetDetailsForCreateBnplPaymentInstrument` does not set the
// legal message when the legal message does not parse.
TEST_F(BnplManagerTest,
       OnDidGetDetailsForCreateBnplPaymentInstrument_InvalidLegalMessages) {
  bnpl_manager_->InitBnplFlow(1'000'000, base::DoNothing());
  BnplIssuer unlinked_issuer = test::GetTestUnlinkedBnplIssuer();

  // Set up legal message for testing.
  auto legal_message = std::make_unique<base::Value::Dict>();
  legal_message->Set("line", "dummy");

  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForCreateBnplPaymentInstrument)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, kContextToken,
          std::move(legal_message)));
  OnIssuerSelected(unlinked_issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->context_token,
            kContextToken);
  EXPECT_TRUE(test_api(*bnpl_manager_)
                  .GetOngoingFlowState()
                  ->legal_message_lines.empty());
}

// Tests that update suggestions callback is called when suggestions are shown
// before amount extraction completion.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_SuggestionShownFirstThenAmountExtractionReturned) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
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
  bnpl_manager_->OnAmountExtractionReturned(
      std::optional<uint64_t>{1'234'560'000ULL});
}

// Tests that update suggestions callback is called when suggestions are shown
// after amount extraction completion.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_AmountExtractionReturnedFirstThenSuggestionShown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/true,
      /*extracted_amount=*/std::optional<uint64_t>{1'234'560'000ULL});
}

// Tests that update suggestions callback will not be called if the amount
// extraction engine fails to pass in an valid value.
TEST_F(BnplManagerTest, AddBnplSuggestion_NoAmountPassedIn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  TriggerBnplUpdateSuggestionsFlow(/*expect_suggestions_are_updated=*/false,
                                   /*extracted_amount=*/std::nullopt);
}

// Tests that update suggestions callback will not be called if the extracted
// amount is not supported by available BNPL issuers.
TEST_F(BnplManagerTest, AddBnplSuggestion_AmountNotSupported) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/std::optional<uint64_t>{30'000'000ULL});
}

// Tests that update suggestions callback will not be called if the BNPL
// feature flag is disabled.
TEST_F(BnplManagerTest, AddBnplSuggestion_BnplFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLater});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/std::optional<uint64_t>{1'234'560'000ULL});
}

// Tests that update suggestions callback will not be called if the BNPL
// feature flag `kAutofillEnableBuyNowPayLaterSyncing` is disabled.
TEST_F(BnplManagerTest, AddBnplSuggestion_BnplSyncFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/std::optional<uint64_t>{1'234'560'000ULL});
}

// Tests that update suggestions callback will be called if the extracted
// amount is only supported by Affirm, and the feature flag for BNPL is
// enabled.
TEST_F(BnplManagerTest, AddBnplSuggestion_AmountSupportedByAffirm) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/true,
      /*extracted_amount=*/std::optional<uint64_t>{50'000'000ULL});
}

// Tests that update suggestions callback will be called if the extracted
// amount is only supported by Zip, and the feature flag for BNPL is enabled.
TEST_F(BnplManagerTest, AddBnplSuggestion_AmountSupportedByZip) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/true,
      /*extracted_amount=*/std::optional<uint64_t>{1'234'560'000ULL});
}

// Tests that update suggestions callback is not called when the showing
// suggestions already contains a BNPL entry.
TEST_F(BnplManagerTest, AddBnplSuggestion_SuggestionShownWithBnplEntry) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  base::MockCallback<UpdateSuggestionsCallback> callback;
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kBnplEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  EXPECT_CALL(callback, Run).Times(0);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
  bnpl_manager_->OnAmountExtractionReturned(
      std::optional<uint64_t>{1'234'560'000ULL});
}

// Tests that update suggestions callback is not called when the BNPL manager
// does not know suggestion generation started.
TEST_F(BnplManagerTest, AddBnplSuggestion_BnplManagerNotNotified) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  base::MockCallback<UpdateSuggestionsCallback> callback;
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  EXPECT_CALL(callback, Run).Times(0);

  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
  bnpl_manager_->OnAmountExtractionReturned(
      std::optional<uint64_t>{1'234'560'000ULL});
}

// Tests that BNPL settings toggle should not be shown if all BNPL
// feature flags are disabled.
TEST_F(BnplManagerTest, BnplSettingsToggleNotShown_BnplFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  // Enable `HasSeenBnpl` flag by generating BNPL suggestion.
  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/true,
      /*extracted_amount=*/std::optional<uint64_t>{1'234'560'000ULL});

  EXPECT_TRUE(bnpl_manager_->ShouldShowBnplSettings());

  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                             features::kAutofillEnableBuyNowPayLater});

  EXPECT_FALSE(bnpl_manager_->ShouldShowBnplSettings());
}

// Tests that BNPL settings toggle should not be shown if BNPL
// issuer feature flags are disabled.
TEST_F(BnplManagerTest, BnplSettingsToggleNotShown_BnplIssuerFeaturesDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  // Enable `HasSeenBnpl` flag by generating BNPL suggestion.
  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/true,
      /*extracted_amount=*/std::optional<uint64_t>{1'234'560'000ULL});

  EXPECT_TRUE(bnpl_manager_->ShouldShowBnplSettings());

  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLater});

  EXPECT_FALSE(bnpl_manager_->ShouldShowBnplSettings());
}

// Tests that BNPL settings toggle should be shown only after BNPL suggestions
// have been generated before.
TEST_F(BnplManagerTest, BnplSettingsToggleNotShown_HasSeenBnpl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(40, 1000, std::string(kBnplAffirmIssuerId), 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, std::string(kBnplZipIssuerId));

  EXPECT_FALSE(autofill_client_->GetPersonalDataManager()
                   .payments_data_manager()
                   .IsAutofillHasSeenBnplPrefEnabled());
  EXPECT_FALSE(bnpl_manager_->ShouldShowBnplSettings());

  // Enable `HasSeenBnpl` flag by generating BNPL suggestion.
  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/true,
      /*extracted_amount=*/std::optional<uint64_t>{1'234'560'000ULL});

  EXPECT_TRUE(autofill_client_->GetPersonalDataManager()
                  .payments_data_manager()
                  .IsAutofillHasSeenBnplPrefEnabled());
  EXPECT_TRUE(bnpl_manager_->ShouldShowBnplSettings());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace autofill::payments
