// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/payments/bnpl_manager_test_api.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/test/mock_payments_window_manager.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill {
class MockCreditCardFormEventLogger
    : public autofill_metrics::CreditCardFormEventLogger {
 public:
  using autofill_metrics::CreditCardFormEventLogger::CreditCardFormEventLogger;
  MOCK_METHOD(void, OnBnplSuggestionShown, (), (override));
};

class MockBrowserAutofillManager : public TestBrowserAutofillManager {
 public:
  explicit MockBrowserAutofillManager(TestAutofillDriver* driver)
      : TestBrowserAutofillManager(driver) {}
  MockBrowserAutofillManager(const MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(const MockBrowserAutofillManager&) =
      delete;
  ~MockBrowserAutofillManager() override = default;

  MOCK_METHOD(autofill_metrics::CreditCardFormEventLogger&,
              GetCreditCardFormEventLogger,
              (),
              (override));
};
}  // namespace autofill

namespace autofill::payments {

using IssuerId = autofill::BnplIssuer::IssuerId;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::InSequence;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::Test;

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
                               std::string instrument_id)> callback));
  MOCK_METHOD(
      void,
      GetDetailsForCreateBnplPaymentInstrument,
      (const GetDetailsForCreateBnplPaymentInstrumentRequestDetails&,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                               std::string context_token,
                               LegalMessageLines)>));
  MOCK_METHOD(
      void,
      GetBnplPaymentInstrumentForFetchingUrl,
      (GetBnplPaymentInstrumentForFetchingUrlRequestDetails request_details,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                               const BnplFetchUrlResponseDetails&)> callback));
};

class TestPaymentsAutofillClientMock : public TestPaymentsAutofillClient {
 public:
  explicit TestPaymentsAutofillClientMock(AutofillClient* client)
      : TestPaymentsAutofillClient(client) {}
  ~TestPaymentsAutofillClientMock() override = default;

  MOCK_METHOD(void,
              ShowBnplTos,
              (BnplTosModel bnpl_tos_model,
               base::OnceClosure accept_callback,
               base::OnceClosure cancel_callback),
              (override));
  MOCK_METHOD(void, CloseBnplTos, (), (override));
  MOCK_METHOD(void,
              ShowSelectBnplIssuerDialog,
              (std::vector<BnplIssuerContext>,
               std::string,
               base::OnceCallback<void(BnplIssuer)>,
               base::OnceClosure),
              (override));
  MOCK_METHOD(void, DismissSelectBnplIssuerDialog, (), (override));
};
}  // namespace

class BnplManagerTest : public Test {
 public:
  const int64_t kBillingCustomerNumber = 1234;
  const std::string kRiskData = "RISK_DATA";
  const std::string kInstrumentId = "INSTRUMENT_ID";
  const std::string kContextToken = "CONTEXT_TOKEN";
  const GURL kRedirectUrl = GURL("REDIRECT_URL");
  const GURL kPopupUrl = GURL("https://test.url/sometestpath/");
  const std::string kAppLocale = "en-GB";
  const std::u16string kLegalMessage = u"LEGAL_MESSAGE";
  const std::string kCurrency = "USD";
  const GURL kDomain = GURL("https://dummytest.com/somepathforurl");
  const uint64_t kAmount = 1'000'000;

  Matcher<BnplIssuerContext> EqualsBnplIssuerContext(
      IssuerId issuer_id,
      BnplIssuerEligibilityForPage eligibility) {
    return AllOf(Field(&BnplIssuerContext::issuer,
                       Property(&BnplIssuer::issuer_id, Eq(issuer_id))),
                 Field(&BnplIssuerContext::eligibility, eligibility));
  }

  BnplManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing,
                              features::kAutofillEnableBuyNowPayLater},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    autofill_client_ = std::make_unique<TestAutofillClient>();
    autofill_client_->SetPrefs(test::PrefServiceForTesting());
    autofill_client_->set_app_locale(kAppLocale);
    autofill_client_->SetAutofillPaymentMethodsEnabled(true);
    autofill_client_->set_last_committed_primary_main_frame_url(kDomain);
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

    autofill_client_->set_payments_autofill_client(
        std::make_unique<TestPaymentsAutofillClientMock>(
            autofill_client_.get()));
    autofill_client_->GetPaymentsAutofillClient()
        ->set_payments_network_interface(std::move(payments_network_interface));
    autofill_driver_ =
        std::make_unique<TestAutofillDriver>(autofill_client_.get());
    auto mock_manager_unique_ptr =
        std::make_unique<NiceMock<MockBrowserAutofillManager>>(
            autofill_driver_.get());
    credit_card_form_event_logger_ =
        std::make_unique<NiceMock<autofill::MockCreditCardFormEventLogger>>(
            mock_manager_unique_ptr.get());

    ON_CALL(*mock_manager_unique_ptr, GetCreditCardFormEventLogger())
        .WillByDefault(ReturnRef(*credit_card_form_event_logger_));

    autofill_driver_->set_autofill_manager(std::move(mock_manager_unique_ptr));
    bnpl_manager_ =
        std::make_unique<BnplManager>(static_cast<BrowserAutofillManager*>(
            &autofill_driver_->GetAutofillManager()));

    ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                autofill_client_->GetAutofillOptimizationGuide()),
            IsUrlEligibleForBnplIssuer)
        .WillByDefault(Return(true));
  }

  // Sets up the PersonalDataManager with a unlinked bnpl issuer.
  void SetUpUnlinkedBnplIssuer(uint64_t price_lower_bound_in_micros,
                               uint64_t price_higher_bound_in_micros,
                               IssuerId issuer_id) {
    std::vector<BnplIssuer::EligiblePriceRange> eligible_price_ranges;
    eligible_price_ranges.emplace_back(kCurrency, price_lower_bound_in_micros,
                                       price_higher_bound_in_micros);
    test_api(autofill_client_->GetPersonalDataManager().payments_data_manager())
        .AddBnplIssuer(BnplIssuer(std::nullopt, issuer_id,
                                  std::move(eligible_price_ranges)));
  }

  // Sets up the PersonalDataManager with a linked bnpl issuer.
  void SetUpLinkedBnplIssuer(uint64_t price_lower_bound_in_micros,
                             uint64_t price_higher_bound_in_micros,
                             IssuerId issuer_id,
                             const int64_t instrument_id) {
    std::vector<BnplIssuer::EligiblePriceRange> eligible_price_ranges;
    eligible_price_ranges.emplace_back(kCurrency, price_lower_bound_in_micros,
                                       price_higher_bound_in_micros);

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

  LegalMessageLines GetExpectedLegalMessageLines() {
    return {TestLegalMessageLine(base::UTF16ToUTF8(kLegalMessage))};
  }

  void OnIssuerSelected(const BnplIssuer& selected_issuer) {
    bnpl_manager_->OnIssuerSelected(selected_issuer);
  }

  TestPaymentsAutofillClientMock& GetPaymentsAutofillClient() {
    return *static_cast<TestPaymentsAutofillClientMock*>(
        autofill_client_->GetPaymentsAutofillClient());
  }

  void TearDown() override {
    credit_card_form_event_logger_->OnDestroyed();
    credit_card_form_event_logger_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  std::unique_ptr<autofill::MockCreditCardFormEventLogger>
      credit_card_form_event_logger_;
  std::unique_ptr<BnplManager> bnpl_manager_;
  raw_ptr<PaymentsNetworkInterfaceMock> payments_network_interface_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// BNPL is currently only available for desktop platforms.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Tests that the initial state for a BNPL flow is set when
// BnplManager::OnDidAcceptBnplSuggestion() is triggered.
TEST_F(BnplManagerTest, OnDidAcceptBnplSuggestion_SetsInitialState) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());

  EXPECT_EQ(
      kAmount,
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
// BnplManager::OnDidAcceptBnplSuggestion() is triggered, even if the app locale
// is not "en-US". This helps test that the flow is easily scalable to other app
// locales.
TEST_F(BnplManagerTest,
       OnDidAcceptBnplSuggestion_SetsInitialStateWithDifferentAppLocale) {
  uint64_t final_checkout_amount = 1000000;
  autofill_client_->set_app_locale("en_GB");
  bnpl_manager_->OnDidAcceptBnplSuggestion(final_checkout_amount,
                                           base::DoNothing());

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
  bnpl_manager_->OnDidAcceptBnplSuggestion(/*final_checkout_amount=*/1000000,
                                           base::DoNothing());
  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  std::string test_context_token = "test_context_token";
  BnplIssuer test_issuer = test::GetTestLinkedBnplIssuer();
  ongoing_flow_state->context_token = test_context_token;
  ongoing_flow_state->issuer = test_issuer;
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
                                      autofill::ConvertToBnplIssuerIdString(
                                          test_issuer.issuer_id()),
                                      test_context_token,
                                      /*risk_data=*/_),
                                  /*callback=*/_));
  test_api(*bnpl_manager_).OnTosDialogAccepted();

  EXPECT_FALSE(ongoing_flow_state->risk_data.empty());
}

// Tests that the the user accepting the ToS dialog triggers a
// CreatePaymentInstrument request with the loaded risk data, if it is present.
TEST_F(BnplManagerTest, TosDialogAccepted_PrefetchedRiskDataLoaded) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(/*final_checkout_amount=*/kAmount,
                                           base::DoNothing());
  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  std::string test_context_token = "test_context_token";
  BnplIssuer test_issuer = test::GetTestLinkedBnplIssuer();
  std::string risk_data = ongoing_flow_state->risk_data;
  ongoing_flow_state->context_token = test_context_token;
  ongoing_flow_state->issuer = test_issuer;

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
                                      autofill::ConvertToBnplIssuerIdString(
                                          test_issuer.issuer_id()),
                                      test_context_token, risk_data),
                                  /*callback=*/_));
  test_api(*bnpl_manager_).OnTosDialogAccepted();

  EXPECT_FALSE(ongoing_flow_state->risk_data.empty());

  // Since risk data was cached, it was directly used, thus loading risk data
  // was skipped.
  EXPECT_FALSE(
      autofill_client_->GetPaymentsAutofillClient()->risk_data_loaded());
}

// Tests that FetchVcnDetails calls the payments network interface with the
// request details filled out correctly, and verifies that the VCN is correctly
// filled and the state of BnplManager is reset.
TEST_F(BnplManagerTest, FetchVcnDetails_CallsGetBnplPaymentInstrument) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  // TODO(crbug.com/400500799): Remove test helper method and set arguments from
  // source.
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  test_api(*bnpl_manager_)
      .PopulateManagerWithUserAndBnplIssuerDetails(
          kBillingCustomerNumber,
          base::NumberToString(issuer.payment_instrument()->instrument_id()),
          kRiskData, kContextToken, kRedirectUrl, issuer);
  base::MockCallback<BnplManager::OnBnplVcnFetchedCallback>
      on_bnpl_vcn_fetched_callback;
  test_api(*bnpl_manager_)
      .SetOnBnplVcnFetchedCallback(on_bnpl_vcn_fetched_callback.Get());

  EXPECT_CALL(
      *payments_network_interface_,
      GetBnplPaymentInstrumentForFetchingVcn(
          /*request_details=*/
          FieldsAre(kBillingCustomerNumber,
                    base::NumberToString(
                        issuer.payment_instrument()->instrument_id()),
                    kRiskData, kContextToken, kPopupUrl,
                    autofill::ConvertToBnplIssuerIdString(issuer.issuer_id())),
          /*callback=*/_));

  BnplFetchVcnResponseDetails response_details;
  response_details.pan = "1234";
  response_details.cvv = "123";
  response_details.cardholder_name = "Cercei";
  response_details.expiration_month = "11";
  response_details.expiration_year = "2035";
  // Verify that a successful GetBnplPaymentInstrumentForFetchingVcn request
  // results in a VCN being correctly created from the
  // BnplFetchVcnResponseDetails.
  CreditCard fetched_vcn;
  EXPECT_CALL(on_bnpl_vcn_fetched_callback, Run(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&fetched_vcn));

  EXPECT_NE(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);

  test_api(*bnpl_manager_).FetchVcnDetails(kPopupUrl);
  test_api(*bnpl_manager_)
      .OnVcnDetailsFetched(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                           response_details);

  EXPECT_EQ(fetched_vcn.number(), base::UTF8ToUTF16(response_details.pan));
  EXPECT_EQ(fetched_vcn.record_type(), CreditCard::RecordType::kVirtualCard);
  EXPECT_EQ(fetched_vcn.cvc(), base::UTF8ToUTF16(response_details.cvv));
  EXPECT_EQ(fetched_vcn.issuer_id(),
            autofill::ConvertToBnplIssuerIdString(issuer.issuer_id()));
  EXPECT_EQ(fetched_vcn.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL),
            base::UTF8ToUTF16(response_details.cardholder_name));
  EXPECT_EQ(fetched_vcn.Expiration2DigitMonthAsString(),
            base::UTF8ToUTF16(response_details.expiration_month));
  EXPECT_EQ(fetched_vcn.Expiration4DigitYearAsString(),
            base::UTF8ToUTF16(response_details.expiration_year));
  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that OnVcnDetailsFetched shows an error dialog when there is a
// PaymentsRpcResult error.
TEST_F(BnplManagerTest, FetchVcnDetails_RpcError) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(1'000'000, base::DoNothing());
  // TODO(crbug.com/400500799): Remove test helper method and set arguments from
  // source.
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  test_api(*bnpl_manager_)
      .PopulateManagerWithUserAndBnplIssuerDetails(
          kBillingCustomerNumber,
          base::NumberToString(issuer.payment_instrument()->instrument_id()),
          kRiskData, kContextToken, kRedirectUrl, issuer);
  base::MockCallback<BnplManager::OnBnplVcnFetchedCallback>
      on_bnpl_vcn_fetched_callback;
  test_api(*bnpl_manager_)
      .SetOnBnplVcnFetchedCallback(on_bnpl_vcn_fetched_callback.Get());

  BnplFetchVcnResponseDetails response_details;
  EXPECT_CALL(on_bnpl_vcn_fetched_callback, Run(_)).Times(0);
  EXPECT_NE(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);

  test_api(*bnpl_manager_).FetchVcnDetails(kPopupUrl);
  test_api(*bnpl_manager_)
      .OnVcnDetailsFetched(PaymentsAutofillClient::PaymentsRpcResult::
                               kVcnRetrievalPermanentFailure,
                           response_details);

  EXPECT_TRUE(autofill_client_->GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());
  EXPECT_EQ(autofill_client_->GetPaymentsAutofillClient()
                ->autofill_error_dialog_context(),
            AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                /*is_permanent_error=*/true));

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that `OnIssuerSelected()` calls with a linked BNPL issuer will call
// the payments network interface with the request details filled out correctly.
TEST_F(
    BnplManagerTest,
    OnIssuerSelected_CallsGetBnplPaymentInstrumentForFetchingUrl_LinkedIssuer) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  BnplIssuer linked_issuer = test::GetTestLinkedBnplIssuer();

  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  ongoing_flow_state->risk_data.clear();
  ASSERT_TRUE(ongoing_flow_state->risk_data.empty());

  EXPECT_CALL(
      *payments_network_interface_,
      GetBnplPaymentInstrumentForFetchingUrl(
          /*request_details=*/
          FieldsAre(kBillingCustomerNumber,
                    base::NumberToString(
                        linked_issuer.payment_instrument()->instrument_id()),
                    _, url::Origin::Create(GURL(kDomain)).GetURL(), kAmount,
                    kCurrency),
          /*callback=*/_))
      .Times(1);

  OnIssuerSelected(linked_issuer);

  EXPECT_EQ(ongoing_flow_state->issuer, linked_issuer);
  EXPECT_EQ(ongoing_flow_state->instrument_id,
            base::NumberToString(
                linked_issuer.payment_instrument()->instrument_id()));
  EXPECT_FALSE(ongoing_flow_state->risk_data.empty());
}

// Tests that `OnIssuerSelected()` calls with a linked BNPL issuer will not
// load risk data again when there is a risk data string saved.
TEST_F(
    BnplManagerTest,
    OnIssuerSelected_CallsGetBnplPaymentInstrumentForFetchingUrl_LinkedIssuer_RiskDataLoaded) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  BnplIssuer linked_issuer = test::GetTestLinkedBnplIssuer();

  // Set up risk data cache.
  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  ongoing_flow_state->risk_data = kRiskData;
  ASSERT_FALSE(ongoing_flow_state->risk_data.empty());

  autofill_client_->GetPaymentsAutofillClient()->set_risk_data_loaded(false);

  EXPECT_CALL(
      *payments_network_interface_,
      GetBnplPaymentInstrumentForFetchingUrl(
          /*request_details=*/
          FieldsAre(kBillingCustomerNumber,
                    base::NumberToString(
                        linked_issuer.payment_instrument()->instrument_id()),
                    kRiskData, url::Origin::Create(GURL(kDomain)).GetURL(),
                    kAmount, kCurrency),
          /*callback=*/_))
      .Times(1);

  OnIssuerSelected(linked_issuer);

  EXPECT_EQ(ongoing_flow_state->issuer, linked_issuer);
  EXPECT_EQ(ongoing_flow_state->instrument_id,
            base::NumberToString(
                linked_issuer.payment_instrument()->instrument_id()));
  EXPECT_EQ(ongoing_flow_state->risk_data, kRiskData);

  // Since risk data was cached, it was directly used, thus loading risk data
  // was skipped.
  EXPECT_FALSE(
      autofill_client_->GetPaymentsAutofillClient()->risk_data_loaded());
}

// Tests that the manager set flow state based on the url fetch result and
// init the flow to redirect user to the site of the selected issuer.
TEST_F(BnplManagerTest, OnIssuerSelected_OnRedirectUrlFetched) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  BnplIssuer linked_issuer = test::GetTestLinkedBnplIssuer();

  BnplFetchUrlResponseDetails response;
  response.redirect_url = kRedirectUrl;
  response.success_url_prefix = GURL("success");
  response.failure_url_prefix = GURL("failure");
  response.context_token = kContextToken;

  EXPECT_CALL(*static_cast<MockPaymentsWindowManager*>(
                  autofill_client_->GetPaymentsAutofillClient()
                      ->GetPaymentsWindowManager()),
              InitBnplFlow(FieldsAre(linked_issuer.issuer_id(), kRedirectUrl,
                                     response.success_url_prefix,
                                     response.failure_url_prefix,
                                     /*completion_callback=*/_)))
      .Times(1);
  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingUrl)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, response));

  OnIssuerSelected(linked_issuer);

  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  EXPECT_EQ(ongoing_flow_state->issuer, linked_issuer);
  EXPECT_EQ(ongoing_flow_state->context_token, kContextToken);
  EXPECT_EQ(ongoing_flow_state->redirect_url, kRedirectUrl);
}

// Tests that the error message is shown when redirect url fetch fails with a
// temporary error.
TEST_F(BnplManagerTest,
       OnIssuerSelected_OnRedirectUrlFetched_TemporaryFailure) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  BnplIssuer linked_issuer = test::GetTestLinkedBnplIssuer();

  BnplFetchUrlResponseDetails response;
  response.redirect_url = kRedirectUrl;
  response.success_url_prefix = GURL("success");
  response.failure_url_prefix = GURL("failure");
  response.context_token = kContextToken;

  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingUrl)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kTryAgainFailure,
          response));

  OnIssuerSelected(linked_issuer);

  EXPECT_TRUE(autofill_client_->GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());
  EXPECT_EQ(autofill_client_->GetPaymentsAutofillClient()
                ->autofill_error_dialog_context(),
            AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                /*is_permanent_error=*/false));
  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that the error message is shown when redirect url fetch fails.
TEST_F(BnplManagerTest,
       OnIssuerSelected_OnRedirectUrlFetched_PermanentFailure) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  BnplIssuer linked_issuer = test::GetTestLinkedBnplIssuer();

  BnplFetchUrlResponseDetails response;
  response.redirect_url = kRedirectUrl;
  response.success_url_prefix = GURL("success");
  response.failure_url_prefix = GURL("failure");
  response.context_token = kContextToken;

  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingUrl)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::
              kVcnRetrievalPermanentFailure,
          response));

  OnIssuerSelected(linked_issuer);

  EXPECT_TRUE(autofill_client_->GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());
  EXPECT_EQ(autofill_client_->GetPaymentsAutofillClient()
                ->autofill_error_dialog_context(),
            AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                /*is_permanent_error=*/true));
  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that when BNPL flow completed successfully, the manager will attempt to
// fetch VCN.
TEST_F(BnplManagerTest, OnPopupWindowCompleted_WithSuccess) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());

  // Init the `PaymentsWindowManager` BNPL flow.
  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingUrl)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
          BnplFetchUrlResponseDetails()));
  BnplIssuer linked_issuer = test::GetTestLinkedBnplIssuer();
  auto& payments_window_manager = *static_cast<MockPaymentsWindowManager*>(
      autofill_client_->GetPaymentsAutofillClient()
          ->GetPaymentsWindowManager());

  EXPECT_CALL(payments_window_manager, InitBnplFlow)
      .WillOnce([&](PaymentsWindowManager::BnplContext bnpl_context) {
        std::move(bnpl_context.completion_callback)
            .Run(PaymentsWindowManager::BnplFlowResult::kSuccess, kPopupUrl);
      });

  GetBnplPaymentInstrumentForFetchingVcnRequestDetails request_details;
  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingVcn)
      .Times(1)
      .WillOnce(SaveArg<0>(&request_details));

  OnIssuerSelected(linked_issuer);

  EXPECT_EQ(request_details.billing_customer_number, kBillingCustomerNumber);
  EXPECT_EQ(request_details.issuer_id,
            autofill::ConvertToBnplIssuerIdString(linked_issuer.issuer_id()));
  EXPECT_EQ(request_details.redirect_url, kPopupUrl);
  EXPECT_EQ(request_details.risk_data, "some risk data");
}

// Tests that when BNPL flow completed with user closed, the flow status will
// be reset.
TEST_F(BnplManagerTest, OnPopupWindowCompleted_UserClosed) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());

  // Init the `PaymentsWindowManager` BNPL flow.
  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingUrl)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
          BnplFetchUrlResponseDetails()));
  BnplIssuer linked_issuer = test::GetTestLinkedBnplIssuer();
  auto& payments_window_manager = *static_cast<MockPaymentsWindowManager*>(
      autofill_client_->GetPaymentsAutofillClient()
          ->GetPaymentsWindowManager());

  EXPECT_CALL(payments_window_manager, InitBnplFlow)
      .WillOnce([&](PaymentsWindowManager::BnplContext bnpl_context) {
        std::move(bnpl_context.completion_callback)
            .Run(PaymentsWindowManager::BnplFlowResult::kUserClosed, kPopupUrl);
      });

  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingVcn)
      .Times(0);

  OnIssuerSelected(linked_issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that when BNPL flow completed with failure, the error message is shown.
TEST_F(BnplManagerTest, OnPopupWindowCompleted_Failure) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());

  // Init the `PaymentsWindowManager` BNPL flow.
  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingUrl)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
          BnplFetchUrlResponseDetails()));
  BnplIssuer linked_issuer = test::GetTestLinkedBnplIssuer();
  auto& payments_window_manager = *static_cast<MockPaymentsWindowManager*>(
      autofill_client_->GetPaymentsAutofillClient()
          ->GetPaymentsWindowManager());

  EXPECT_CALL(payments_window_manager, InitBnplFlow)
      .WillOnce([&](PaymentsWindowManager::BnplContext bnpl_context) {
        std::move(bnpl_context.completion_callback)
            .Run(PaymentsWindowManager::BnplFlowResult::kFailure, kPopupUrl);
      });

  OnIssuerSelected(linked_issuer);

  EXPECT_TRUE(autofill_client_->GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());
  EXPECT_EQ(autofill_client_->GetPaymentsAutofillClient()
                ->autofill_error_dialog_context(),
            AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                /*is_permanent_error=*/false));
  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that FetchVcnDetails will display an autofill progress dialog.
TEST_F(BnplManagerTest, FetchVcnDetails_ShowAutofillProgressDialog) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(1'000'000, base::DoNothing());
  test_api(*bnpl_manager_)
      .PopulateManagerWithUserAndBnplIssuerDetails(
          kBillingCustomerNumber, kInstrumentId, kRiskData, kContextToken,
          kRedirectUrl, test::GetTestLinkedBnplIssuer());

  EXPECT_FALSE(autofill_client_->GetPaymentsAutofillClient()
                   ->autofill_progress_dialog_shown());
  EXPECT_FALSE(autofill_client_->GetPaymentsAutofillClient()
                   ->autofill_error_dialog_shown());

  test_api(*bnpl_manager_).FetchVcnDetails(kPopupUrl);

  EXPECT_TRUE(autofill_client_->GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());
  EXPECT_FALSE(autofill_client_->GetPaymentsAutofillClient()
                   ->autofill_error_dialog_shown());
}

// Tests that calling Reset while fetching VCN details will reset the status of
// BnplManager.
TEST_F(BnplManagerTest, FetchVcnDetails_Reset) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(1'000'000, base::DoNothing());
  test_api(*bnpl_manager_)
      .PopulateManagerWithUserAndBnplIssuerDetails(
          kBillingCustomerNumber, kInstrumentId, kRiskData, kContextToken,
          kRedirectUrl, test::GetTestLinkedBnplIssuer());

  EXPECT_FALSE(autofill_client_->GetPaymentsAutofillClient()
                   ->autofill_progress_dialog_shown());
  EXPECT_FALSE(autofill_client_->GetPaymentsAutofillClient()
                   ->autofill_error_dialog_shown());
  EXPECT_NE(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);

  test_api(*bnpl_manager_).FetchVcnDetails(kPopupUrl);

  EXPECT_TRUE(autofill_client_->GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());
  EXPECT_FALSE(autofill_client_->GetPaymentsAutofillClient()
                   ->autofill_error_dialog_shown());
  EXPECT_NE(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);

  test_api(*bnpl_manager_).Reset();

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
  EXPECT_FALSE(autofill_client_->GetPaymentsAutofillClient()
                   ->autofill_error_dialog_shown());
}

// Tests that `OnIssuerSelected()` calls with an unlinked BNPL issuer will call
// the payments network interface with the request details filled out correctly.
TEST_F(
    BnplManagerTest,
    OnIssuerSelected_CallsGetDetailsForCreateBnplPaymentInstrument_UnlinkedIssuer) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());

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
                            autofill::ConvertToBnplIssuerIdString(
                                unlinked_issuer.issuer_id())),
                  /*callback=*/_))
      .Times(1);

  OnIssuerSelected(unlinked_issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->issuer,
            unlinked_issuer);
}

// Tests that `OnDidGetDetailsForCreateBnplPaymentInstrument` set the BNPL
// manager state if the request has completed successfully, and shows the ToS
// dialog. This test also ensures the ToS dialog is closed after receiving a
// redirect URL for an unlinked issuer.
TEST_F(
    BnplManagerTest,
    OnDidGetDetailsForCreateBnplPaymentInstrument_ClosesTosAfterRedirectUrlReceived) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(1'000'000, base::DoNothing());
  BnplIssuer unlinked_issuer = test::GetTestUnlinkedBnplIssuer();

  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForCreateBnplPaymentInstrument)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, kContextToken,
          GetExpectedLegalMessageLines()));

  BnplTosModel bnpl_tos_model;
  EXPECT_CALL(*static_cast<TestPaymentsAutofillClientMock*>(
                  autofill_client_->GetPaymentsAutofillClient()),
              ShowBnplTos)
      .WillOnce(SaveArg<0>(&bnpl_tos_model));
  OnIssuerSelected(unlinked_issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->context_token,
            kContextToken);

  const LegalMessageLines& legal_message_lines =
      bnpl_tos_model.legal_message_lines;
  ASSERT_FALSE(legal_message_lines.empty());
  EXPECT_EQ(legal_message_lines[0].text(), kLegalMessage);

  EXPECT_EQ(bnpl_tos_model.issuer, unlinked_issuer);

  EXPECT_CALL(*static_cast<TestPaymentsAutofillClientMock*>(
                  autofill_client_->GetPaymentsAutofillClient()),
              CloseBnplTos);

  test_api(*bnpl_manager_)
      .OnRedirectUrlFetched(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                            BnplFetchUrlResponseDetails());
}

// Tests that cancelling the ToS dialog resets and ends the flow.
TEST_F(
    BnplManagerTest,
    OnDidGetDetailsForCreateBnplPaymentInstrument_TosCancellationResetsFlow) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(1'000'000, base::DoNothing());
  BnplIssuer unlinked_issuer = test::GetTestUnlinkedBnplIssuer();

  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForCreateBnplPaymentInstrument)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, kContextToken,
          GetExpectedLegalMessageLines()));

  // Cancel the ToS dialog by running the cancel callback (2nd param).
  EXPECT_CALL(*static_cast<TestPaymentsAutofillClientMock*>(
                  autofill_client_->GetPaymentsAutofillClient()),
              ShowBnplTos)
      .WillOnce(base::test::RunOnceCallback<2>());

  EXPECT_NE(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);

  OnIssuerSelected(unlinked_issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that `OnDidGetDetailsForCreateBnplPaymentInstrument` shows an error
// when there is a PaymentsRpcResult error.
TEST_F(BnplManagerTest,
       OnDidGetDetailsForCreateBnplPaymentInstrument_RpcError) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(1'000'000, base::DoNothing());
  BnplIssuer unlinked_issuer = test::GetTestUnlinkedBnplIssuer();

  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForCreateBnplPaymentInstrument)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kTryAgainFailure,
          kContextToken, GetExpectedLegalMessageLines()));
  OnIssuerSelected(unlinked_issuer);

  EXPECT_TRUE(autofill_client_->GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());
  EXPECT_EQ(autofill_client_->GetPaymentsAutofillClient()
                ->autofill_error_dialog_context(),
            AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                /*is_permanent_error=*/false));
  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that `OnDidAcceptBnplSuggestion()` show trigger
// `ShowSelectBnplIssuerDialog()` call.
TEST_F(BnplManagerTest, OnDidAcceptBnplSuggestion_ShowSelectBnplIssuerDialog) {
  EXPECT_CALL(GetPaymentsAutofillClient(), ShowSelectBnplIssuerDialog);

  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
}

// Tests that the BNPL flow will be reset if the user cancels the select issuer
// dialog.
TEST_F(BnplManagerTest, ShowSelectBnplIssuerDialog_UserCancelled) {
  InSequence s;
  EXPECT_CALL(GetPaymentsAutofillClient(), ShowSelectBnplIssuerDialog)
      .WillOnce(base::test::RunOnceCallback<3>());

  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that `OnDidGetDetailsForCreateBnplPaymentInstrument` will dismiss
// the showing issuer selection dialog.
TEST_F(
    BnplManagerTest,
    OnDidGetDetailsForCreateBnplPaymentInstrument_DismissSelectBnplIssuerDialog) {
  const BnplIssuer unlinked_issuer = test::GetTestUnlinkedBnplIssuer();

  InSequence s;
  EXPECT_CALL(GetPaymentsAutofillClient(), ShowSelectBnplIssuerDialog)
      .WillOnce(base::test::RunOnceCallback<2>(unlinked_issuer));
  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForCreateBnplPaymentInstrument)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, kContextToken,
          GetExpectedLegalMessageLines()));
  EXPECT_CALL(GetPaymentsAutofillClient(), DismissSelectBnplIssuerDialog);

  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->issuer,
            unlinked_issuer);
}

// Tests that `OnRedirectUrlFetched` will will dismiss the showing issuer
// selection dialog.
TEST_F(BnplManagerTest,
       OnRedirectUrlFetched_LinkedIssuer_DismissSelectBnplIssuerDialog) {
  BnplFetchUrlResponseDetails response;
  response.redirect_url = kRedirectUrl;
  response.success_url_prefix = GURL("success");
  response.failure_url_prefix = GURL("failure");
  response.context_token = kContextToken;

  InSequence s;
  EXPECT_CALL(GetPaymentsAutofillClient(), ShowSelectBnplIssuerDialog)
      .WillOnce(
          base::test::RunOnceCallback<2>(test::GetTestLinkedBnplIssuer()));
  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingUrl)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, response));
  EXPECT_CALL(GetPaymentsAutofillClient(), DismissSelectBnplIssuerDialog);

  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
}

// Tests that update suggestions callback is called when suggestions are shown
// before amount extraction completion.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_SuggestionShownFirstThenAmountExtractionReturned) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  base::MockCallback<UpdateSuggestionsCallback> callback;
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  EXPECT_CALL(callback, Run).Times(1);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
  bnpl_manager_->OnAmountExtractionReturned(1'234'560'000ULL);
}

// Tests that update suggestions callback is called when suggestions are shown
// after amount extraction completion.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_AmountExtractionReturnedFirstThenSuggestionShown) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/true,
      /*extracted_amount=*/1'234'560'000ULL);
}

// Tests that `IsEligibleForBnpl()` returns false if the client does not have
// an `AutofillOptimizationGuide` assigned.
TEST_F(BnplManagerTest, IsEligibleForBnpl_NoAutofillOptimizationGuide) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm, /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  autofill_client_->ResetAutofillOptimizationGuide();

  EXPECT_FALSE(bnpl_manager_->IsEligibleForBnpl());
}

// Tests that `IsEligibleForBnpl()` returns false if if the current visiting
// url is not in the allowlist.
TEST_F(BnplManagerTest, IsEligibleForBnpl_UrlNotSupported) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm, /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_->GetAutofillOptimizationGuide()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(false));

  EXPECT_FALSE(bnpl_manager_->IsEligibleForBnpl());
}

// Tests that when the current visiting url is only supported by one of the
// BNPL issuers, `IsEligibleForBnpl()` returns true.
TEST_F(BnplManagerTest, IsEligibleForBnpl_UrlSupportedByOneIssuer) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm, /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_->GetAutofillOptimizationGuide()),
          IsUrlEligibleForBnplIssuer(IssuerId::kBnplAffirm, _))
      .WillByDefault(Return(false));
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_->GetAutofillOptimizationGuide()),
          IsUrlEligibleForBnplIssuer(IssuerId::kBnplZip, _))
      .WillByDefault(Return(true));

  EXPECT_TRUE(bnpl_manager_->IsEligibleForBnpl());
}

// Tests that update suggestions callback will not be called if the amount
// extraction engine fails to pass in an valid value.
TEST_F(BnplManagerTest, AddBnplSuggestion_NoAmountPassedIn) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  TriggerBnplUpdateSuggestionsFlow(/*expect_suggestions_are_updated=*/false,
                                   /*extracted_amount=*/std::nullopt);
}

// Tests that BnplSuggestionNotShownReason will be logged once if the amount
// extraction engine fails to pass in a valid value.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_NoAmountPassedIn_BnplSuggestionNotShownReasonLogged) {
  base::HistogramTester histogram_tester;

  // Add one linked issuer to payments data manager.
  SetUpLinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/40,
      /*price_higher_bound_in_micros=*/1000, IssuerId::kBnplAffirm,
      /*instrument_id=*/1234);

  TriggerBnplUpdateSuggestionsFlow(/*expect_suggestions_are_updated=*/false,
                                   /*extracted_amount=*/std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionNotShownReason",
      autofill_metrics::BnplSuggestionNotShownReason::kAmountExtractionFailure,
      1);

  // Test that BnplSuggestionNotShownReason is logged only once even if BNPL
  // flow is triggered and not shown more than once on the same page.
  TriggerBnplUpdateSuggestionsFlow(/*expect_suggestions_are_updated=*/false,
                                   /*extracted_amount=*/std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionNotShownReason",
      autofill_metrics::BnplSuggestionNotShownReason::kAmountExtractionFailure,
      1);
}

// Tests that BnplSuggestionNotShownReason will not be logged if BNPL feature
// flag is disabled and the amount extraction engine fails to pass in a valid
// value.
TEST_F(
    BnplManagerTest,
    AddBnplSuggestion_NoAmountPassedIn_BnplSuggestionNotShownReasonNotLogged_BnplDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLater});

  base::HistogramTester histogram_tester;

  // Add one linked issuer to payments data manager.
  SetUpLinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/40,
      /*price_higher_bound_in_micros=*/1000, IssuerId::kBnplAffirm,
      /*instrument_id=*/1234);

  TriggerBnplUpdateSuggestionsFlow(/*expect_suggestions_are_updated=*/false,
                                   /*extracted_amount=*/std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionNotShownReason",
      autofill_metrics::BnplSuggestionNotShownReason::kAmountExtractionFailure,
      0);
}

// Tests that update suggestions callback will not be called if the extracted
// amount is not supported by available BNPL issuers.
TEST_F(BnplManagerTest, AddBnplSuggestion_AmountNotSupported) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/30'000'000ULL);
}

// Tests that BnplSuggestionNotShownReason will be logged once if the extracted
// amount is too high and is not supported by available BNPL issuers.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_AmountTooHigh_BnplSuggestionNotShownReasonLogged) {
  base::HistogramTester histogram_tester;

  // Add one linked issuer to payments data manager.
  SetUpLinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/40,
      /*price_higher_bound_in_micros=*/1000, IssuerId::kBnplAffirm,
      /*instrument_id=*/1234);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/30'000'000ULL);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionNotShownReason",
      autofill_metrics::BnplSuggestionNotShownReason::
          kCheckoutAmountNotSupported,
      1);

  // Test that BnplSuggestionNotShownReason is logged only once even if BNPL
  // flow is triggered and not shown more than once on the same page.
  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/30'000'000ULL);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionNotShownReason",
      autofill_metrics::BnplSuggestionNotShownReason::
          kCheckoutAmountNotSupported,
      1);
}

// Tests that BnplSuggestionNotShownReason will be logged once if the extracted
// amount is too low and is not supported by available BNPL issuers.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_AmountTooLow_BnplSuggestionNotShownReasonLogged) {
  base::HistogramTester histogram_tester;

  // Add one linked issuer to payments data manager.
  SetUpLinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/40,
      /*price_higher_bound_in_micros=*/1000, IssuerId::kBnplAffirm,
      /*instrument_id=*/1234);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/20ULL);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionNotShownReason",
      autofill_metrics::BnplSuggestionNotShownReason::
          kCheckoutAmountNotSupported,
      1);

  // Test that BnplSuggestionNotShownReason is logged only once even if BNPL
  // flow is triggered and not shown more than once on the same page.
  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/20ULL);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionNotShownReason",
      autofill_metrics::BnplSuggestionNotShownReason::
          kCheckoutAmountNotSupported,
      1);
}

// Tests that BnplSuggestionNotShownReason will not be logged if BNPL feature
// flag is disabled and the extracted amount is not supported by available
// BNPL issuers.
TEST_F(
    BnplManagerTest,
    AddBnplSuggestion_AmountNotSupported_BnplSuggestionNotShownReasonNotLogged_BnplDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLater});

  base::HistogramTester histogram_tester;

  // Add one linked issuer to payments data manager.
  SetUpLinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/40,
      /*price_higher_bound_in_micros=*/1000, IssuerId::kBnplAffirm,
      /*instrument_id=*/1234);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/30'000'000ULL);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionNotShownReason",
      autofill_metrics::BnplSuggestionNotShownReason::
          kCheckoutAmountNotSupported,
      0);
}

// Tests that update suggestions callback will not be called if the BNPL
// feature flag is disabled.
TEST_F(BnplManagerTest, AddBnplSuggestion_BnplFeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLater});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/1'234'560'000ULL);
}

// Tests that update suggestions callback will not be called if the BNPL
// feature flag `kAutofillEnableBuyNowPayLaterSyncing` is disabled.
TEST_F(BnplManagerTest, AddBnplSuggestion_BnplSyncFeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing});

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/1'234'560'000ULL);
}

// Tests that update suggestions callback will not be called if the BNPL
// user preference is disabled.
TEST_F(BnplManagerTest, AddBnplSuggestion_BnplPrefDisabled) {
  prefs::SetAutofillBnplEnabled(autofill_client_->GetPrefs(), false);

  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/1'234'560'000ULL);
}

// Tests that update suggestions callback will be called if the extracted
// amount is only supported by Affirm, and the feature flag for BNPL is
// enabled.
TEST_F(BnplManagerTest, AddBnplSuggestion_AmountSupportedByAffirm) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/true,
      /*extracted_amount=*/50'000'000ULL);
}

// Tests that update suggestions callback will be called if the extracted
// amount is only supported by Zip, and the feature flag for BNPL is enabled.
TEST_F(BnplManagerTest, AddBnplSuggestion_AmountSupportedByZip) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/true,
      /*extracted_amount=*/1'234'560'000ULL);
}

// Tests that update suggestions callback is not called when the showing
// suggestions already contains a BNPL entry.
TEST_F(BnplManagerTest, AddBnplSuggestion_SuggestionShownWithBnplEntry) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  base::MockCallback<UpdateSuggestionsCallback> callback;
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kBnplEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  EXPECT_CALL(callback, Run).Times(0);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
  bnpl_manager_->OnAmountExtractionReturned(1'234'560'000ULL);
}

// Tests that update suggestions callback is not called when the BNPL manager
// does not know suggestion generation started.
TEST_F(BnplManagerTest, AddBnplSuggestion_BnplManagerNotNotified) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);

  base::MockCallback<UpdateSuggestionsCallback> callback;
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kManageCreditCard)};
  EXPECT_CALL(callback, Run).Times(0);

  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
  bnpl_manager_->OnAmountExtractionReturned(1'234'560'000ULL);
}

// Tests that when CreateBnplPaymentInstrument and responds with a success
// response, expecting GetBnplPaymentInstrumentForFetchingUrl call with the
// returned instrument ID.
TEST_F(BnplManagerTest, CreateBnplPaymentInstrument_Success) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  ongoing_flow_state->app_locale = kAppLocale;
  ongoing_flow_state->billing_customer_number = kBillingCustomerNumber;
  ongoing_flow_state->context_token = kContextToken;
  ongoing_flow_state->issuer = test::GetTestLinkedBnplIssuer();
  ongoing_flow_state->risk_data = kRiskData;

  EXPECT_CALL(*payments_network_interface_,
              CreateBnplPaymentInstrument(
                  FieldsAre(kAppLocale, kBillingCustomerNumber,
                            autofill::ConvertToBnplIssuerIdString(
                                ongoing_flow_state->issuer.issuer_id()),
                            kContextToken, kRiskData),
                  _))
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, kInstrumentId));

  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingUrl(
                  FieldsAre(kBillingCustomerNumber, kInstrumentId, kRiskData,
                            url::Origin::Create(GURL(kDomain)).GetURL(),
                            kAmount, kCurrency),
                  _))
      .Times(1);

  test_api(*bnpl_manager_).CreateBnplPaymentInstrument();

  EXPECT_EQ(ongoing_flow_state->instrument_id, kInstrumentId);
}

// Tests that when CreateBnplPaymentInstrument fails with an error the error
// dialog is shown and the flow is reset.
TEST_F(BnplManagerTest, CreateBnplPaymentInstrument_Failure) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  ongoing_flow_state->app_locale = kAppLocale;
  ongoing_flow_state->billing_customer_number = kBillingCustomerNumber;
  ongoing_flow_state->context_token = kContextToken;
  ongoing_flow_state->issuer = test::GetTestLinkedBnplIssuer();
  ongoing_flow_state->risk_data = kRiskData;

  EXPECT_CALL(*payments_network_interface_,
              CreateBnplPaymentInstrument(
                  FieldsAre(kAppLocale, kBillingCustomerNumber,
                            autofill::ConvertToBnplIssuerIdString(
                                ongoing_flow_state->issuer.issuer_id()),
                            kContextToken, kRiskData),
                  _))
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure, ""));
  EXPECT_CALL(GetPaymentsAutofillClient(), CloseBnplTos);

  test_api(*bnpl_manager_).CreateBnplPaymentInstrument();

  EXPECT_TRUE(autofill_client_->GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests the sorting logic of `GetSortedBnplIssuerContext` for BNPL Issuers
// based on their linked status and eligibility. The expected order is:
// 1. Linked & Eligible
// 2. Unlinked & Eligible
// 3. Linked & Ineligible
// 4. Unlinked & Ineligible
//
// Two test cases verify this ordering:
// - Test 1(current test): Checks the order of linked eligible, unlinked
// eligible, and linked ineligible.
// - Test 2: Checks the order of unlinked eligible, linked ineligible, and
// unlinked ineligible.
TEST_F(BnplManagerTest, GetSortedBnplIssuerContext_OrdersEligibleFirst) {
  // Unlinked issuer + eligibility: kIsEligible.
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/10'000'000,
                          /*price_higher_bound_in_micros=*/1'000'000'000,
                          IssuerId::kBnplZip);
  // Linked issuer + eligibility: kIsEligible.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/10'000'000,
                        /*price_higher_bound_in_micros=*/3'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1);
  // Linked issuer + eligibility: kNotEligibleIssuerDoesNotSupportMerchant.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/10'000'000,
                        /*price_higher_bound_in_micros=*/200'000'000,
                        IssuerId::kBnplAfterpay,
                        /*instrument_id=*/4);

  // Mock merchant eligibility for issuers based on issuer id.
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_->GetAutofillOptimizationGuide()),
          IsUrlEligibleForBnplIssuer(IssuerId::kBnplAfterpay, _))
      .WillByDefault(Return(false));
  ON_CALL(
      *static_cast<MockAutofillOptimizationGuide*>(
          autofill_client_->GetAutofillOptimizationGuide()),
      IsUrlEligibleForBnplIssuer(
          Matcher<IssuerId>(AnyOf(IssuerId::kBnplAffirm, IssuerId::kBnplZip)),
          _))
      .WillByDefault(Return(true));

  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetPaymentsAutofillClient(), ShowSelectBnplIssuerDialog)
      .WillOnce(MoveArg<0>(&issuer_context));

  bnpl_manager_->OnDidAcceptBnplSuggestion(15'000'000, base::DoNothing());

  EXPECT_THAT(
      issuer_context,
      ElementsAre(
          // Linked eligible issuers.
          EqualsBnplIssuerContext(IssuerId::kBnplAffirm,
                                  BnplIssuerEligibilityForPage::kIsEligible),
          // Unlinked eligible issuers.
          EqualsBnplIssuerContext(IssuerId::kBnplZip,
                                  BnplIssuerEligibilityForPage::kIsEligible),
          // Linked uneligible issuers.
          EqualsBnplIssuerContext(
              IssuerId::kBnplAfterpay,
              BnplIssuerEligibilityForPage::
                  kNotEligibleIssuerDoesNotSupportMerchant)));
}

// Tests the sorting logic of `GetSortedBnplIssuerContext` for BNPL Issuers
// based on their linked status and eligibility. The expected order is:
// 1. Linked & Eligible
// 2. Unlinked & Eligible
// 3. Linked & Ineligible
// 4. Unlinked & Ineligible
//
// Two test cases verify this ordering:
// - Test 1: Checks the order of linked eligible, unlinked eligible, and linked
// ineligible.
// - Test 2(current test): Checks the order of unlinked eligible, linked
// ineligible, and unlinked ineligible.
TEST_F(BnplManagerTest, GetSortedBnplIssuerContext_OrdersUneligibleLast) {
  // Unlinked issuer + eligibility: kIsEligible.
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/10'000'000,
                          /*price_higher_bound_in_micros=*/1'000'000'000,
                          IssuerId::kBnplZip);
  // Linked issuer + eligibility: kNotEligibleIssuerDoesNotSupportMerchant.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/10'000'000,
                        /*price_higher_bound_in_micros=*/200'000'000,
                        IssuerId::kBnplAfterpay,
                        /*instrument_id=*/4);
  // Unlinked issuer + eligibility: kIsEligible.
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/10'000'000,
                          /*price_higher_bound_in_micros=*/1'000'000'000,
                          IssuerId::kBnplAffirm);

  // Mock merchant eligibility for issuers based on issuer id.
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_->GetAutofillOptimizationGuide()),
          IsUrlEligibleForBnplIssuer(
              Matcher<IssuerId>(
                  AnyOf(IssuerId::kBnplAffirm, IssuerId::kBnplAfterpay)),
              _))
      .WillByDefault(Return(false));
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_->GetAutofillOptimizationGuide()),
          IsUrlEligibleForBnplIssuer(IssuerId::kBnplZip, _))
      .WillByDefault(Return(true));

  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetPaymentsAutofillClient(), ShowSelectBnplIssuerDialog)
      .WillOnce(MoveArg<0>(&issuer_context));

  bnpl_manager_->OnDidAcceptBnplSuggestion(15'000'000, base::DoNothing());

  EXPECT_THAT(
      issuer_context,
      ElementsAre(
          // Unlinked eligible issuers.
          EqualsBnplIssuerContext(IssuerId::kBnplZip,
                                  BnplIssuerEligibilityForPage::kIsEligible),
          // Linked uneligible issuers.
          EqualsBnplIssuerContext(IssuerId::kBnplAfterpay,
                                  BnplIssuerEligibilityForPage::
                                      kNotEligibleIssuerDoesNotSupportMerchant),
          // Unlinked uneligible issuers.
          EqualsBnplIssuerContext(
              IssuerId::kBnplAffirm,
              BnplIssuerEligibilityForPage::
                  kNotEligibleIssuerDoesNotSupportMerchant)));
}

// Test that `GetSortedBnplIssuerContext` returns eligible BNPL Issuer with
// eligibility `BnplIssuerEligibilityForPage::kIsEligible`.
TEST_F(BnplManagerTest, GetSortedBnplIssuerContext_IsEligible) {
  SetUpUnlinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/10'000'000,
      /*price_higher_bound_in_micros=*/1'000'000'000, IssuerId::kBnplAfterpay);
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_->GetAutofillOptimizationGuide()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(true));

  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetPaymentsAutofillClient(), ShowSelectBnplIssuerDialog)
      .WillOnce(MoveArg<0>(&issuer_context));

  bnpl_manager_->OnDidAcceptBnplSuggestion(15'000'000, base::DoNothing());

  EXPECT_THAT(issuer_context, ElementsAre(EqualsBnplIssuerContext(
                                  IssuerId::kBnplAfterpay,
                                  BnplIssuerEligibilityForPage::kIsEligible)));
}

// Test that when the BNPL Issuer does not support the current merchant,
// `GetSortedBnplIssuerContext` will returns BnplIssuerContext contains the
// Issuer and
// `BnplIssuerEligibilityForPage::kNotEligibleIssuerDoesNotSupportMerchant`.
TEST_F(BnplManagerTest, GetSortedBnplIssuerContext_NotSupportedMerchant) {
  SetUpUnlinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/10'000'000,
      /*price_higher_bound_in_micros=*/1'000'000'000, IssuerId::kBnplAfterpay);

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_->GetAutofillOptimizationGuide()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(false));

  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetPaymentsAutofillClient(), ShowSelectBnplIssuerDialog)
      .WillOnce(MoveArg<0>(&issuer_context));

  bnpl_manager_->OnDidAcceptBnplSuggestion(15'000'000, base::DoNothing());

  EXPECT_THAT(issuer_context,
              ElementsAre(EqualsBnplIssuerContext(
                  IssuerId::kBnplAfterpay,
                  BnplIssuerEligibilityForPage::
                      kNotEligibleIssuerDoesNotSupportMerchant)));
}

// Test that when checkout amount is too high for the issuer,
// `GetSortedBnplIssuerContext` will returns BnplIssuerContext contains the
// Issuer and `BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh`.
TEST_F(BnplManagerTest, GetSortedBnplIssuerContext_CheckoutAmountTooHigh) {
  SetUpUnlinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/10'000'000,
      /*price_higher_bound_in_micros=*/1'000'000'000, IssuerId::kBnplAfterpay);

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_->GetAutofillOptimizationGuide()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(true));

  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetPaymentsAutofillClient(), ShowSelectBnplIssuerDialog)
      .WillOnce(MoveArg<0>(&issuer_context));

  bnpl_manager_->OnDidAcceptBnplSuggestion(1'001'000'000, base::DoNothing());

  EXPECT_THAT(
      issuer_context,
      ElementsAre(EqualsBnplIssuerContext(
          IssuerId::kBnplAfterpay,
          BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh)));
}

// Test that when checkout amount is too low for the issuer,
// `GetSortedBnplIssuerContext` will returns BnplIssuerContext contains the
// Issuer and `BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow`
TEST_F(BnplManagerTest, GetSortedBnplIssuerContext_CheckoutAmountTooLow) {
  SetUpUnlinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/1'002'000'000,
      /*price_higher_bound_in_micros=*/2'000'000'000, IssuerId::kBnplAfterpay);

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_->GetAutofillOptimizationGuide()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(true));

  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetPaymentsAutofillClient(), ShowSelectBnplIssuerDialog)
      .WillOnce(MoveArg<0>(&issuer_context));

  bnpl_manager_->OnDidAcceptBnplSuggestion(1'001'000'000, base::DoNothing());

  EXPECT_THAT(
      issuer_context,
      ElementsAre(EqualsBnplIssuerContext(
          IssuerId::kBnplAfterpay,
          BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow)));
}

// Tests that the `kBnplSuggestionAccepted` event is logged once when
// `OnDidAcceptBnplSuggestion()` is called.
TEST_F(BnplManagerTest, OnDidAcceptBnplSuggestion_SuggestionAcceptedLogged) {
  base::HistogramTester histogram_tester;

  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionAccepted,
      /*expected_bucket_count=*/1);

  // Test that `kBnplSuggestionAccepted` is logged only once even if
  // `OnDidAcceptBnplSuggestion()` is called more than once on the same page.
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  histogram_tester.ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionAccepted,
      /*expected_bucket_count=*/1);
}

TEST_F(BnplManagerTest,
       AddBnplSuggestion_SuggestionUpdatedAndOnBnplSuggestionShownCalled) {
  // Add one linked issuer and one unlinked issuer to payments data manager.
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/40'000'000,
                        /*price_higher_bound_in_micros=*/1'000'000'000,
                        BnplIssuer::IssuerId::kBnplAffirm,
                        /*instrument_id=*/1234);
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          BnplIssuer::IssuerId::kBnplZip);

  base::MockCallback<UpdateSuggestionsCallback> callback;
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kManageCreditCard)};

  EXPECT_CALL(callback, Run);
  EXPECT_CALL(*credit_card_form_event_logger_, OnBnplSuggestionShown());

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
  bnpl_manager_->OnAmountExtractionReturned(50'000'000ULL);
}

TEST_F(
    BnplManagerTest,
    AddBnplSuggestion_SuggestionNotUpdatedAndOnBnplSuggestionShownNotCalled) {
  SetUpLinkedBnplIssuer(40, 1000, BnplIssuer::IssuerId::kBnplAffirm, 1234);
  SetUpUnlinkedBnplIssuer(1000, 2000, BnplIssuer::IssuerId::kBnplZip);

  base::MockCallback<UpdateSuggestionsCallback> callback;

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kBnplEntry),
      Suggestion(SuggestionType::kManageCreditCard)};

  EXPECT_CALL(callback, Run).Times(0);
  EXPECT_CALL(*credit_card_form_event_logger_, OnBnplSuggestionShown())
      .Times(0);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
  bnpl_manager_->OnAmountExtractionReturned(1'234'560'000ULL);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace autofill::payments
