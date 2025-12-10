// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_manager.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
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
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/mock_autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/payments/bnpl_manager_test_api.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
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
#include "components/autofill/core/browser/ui/payments/bnpl_ui_delegate.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill::payments {

using IssuerId = autofill::BnplIssuer::IssuerId;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::Test;

namespace {
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

  MOCK_METHOD(payments::AmountExtractionManager&,
              GetAmountExtractionManager,
              (),
              (override));
};

class MockAmountExtractionManager : public AmountExtractionManager {
 public:
  explicit MockAmountExtractionManager(BrowserAutofillManager* autofill_manager)
      : AmountExtractionManager(autofill_manager) {}

  MOCK_METHOD(void, TriggerCheckoutAmountExtractionWithAi, (), (override));
};

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
  MOCK_METHOD(
      void,
      UpdateBnplPaymentInstrument,
      (const UpdateBnplPaymentInstrumentRequestDetails& request_details,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
           callback));
  MOCK_METHOD(
      void,
      GetDetailsForUpdateBnplPaymentInstrument,
      (const GetDetailsForUpdateBnplPaymentInstrumentRequestDetails&,
       base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                               std::string context_token,
                               LegalMessageLines)>));
};

class TestPaymentsAutofillClientMock : public TestPaymentsAutofillClient {
 public:
  explicit TestPaymentsAutofillClientMock(AutofillClient* client)
      : TestPaymentsAutofillClient(client) {}
  ~TestPaymentsAutofillClientMock() override = default;

  MOCK_METHOD(
      bool,
      OnPurchaseAmountExtracted,
      (base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
       std::optional<int64_t> extracted_amount,
       bool is_amount_supported_by_any_issuer,
       const std::optional<std::string>& app_locale,
       base::OnceCallback<void(autofill::BnplIssuer)> selected_issuer_callback,
       base::OnceClosure cancel_callback),
      (override));
};

class MockBnplUiDelegate : public BnplUiDelegate {
 public:
  MockBnplUiDelegate() = default;
  ~MockBnplUiDelegate() override = default;

  MOCK_METHOD(void,
              ShowSelectBnplIssuerUi,
              (std::vector<BnplIssuerContext> bnpl_issuer_context,
               std::string app_locale,
               base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
               base::OnceClosure cancel_callback,
               bool has_seen_ai_terms),
              (override));
  MOCK_METHOD(void,
              UpdateBnplIssuerDialogUi,
              (std::vector<BnplIssuerContext> bnpl_issuer_context),
              (override));
  MOCK_METHOD(void, RemoveSelectBnplIssuerOrProgressUi, (), (override));
  MOCK_METHOD(void,
              ShowBnplTosUi,
              (BnplTosModel bnpl_tos_model,
               base::OnceClosure accept_callback,
               base::OnceClosure cancel_callback),
              (override));
  MOCK_METHOD(void, RemoveBnplTosOrProgressUi, (), (override));
  MOCK_METHOD(void,
              ShowProgressUi,
              (AutofillProgressDialogType autofill_progress_dialog_type,
               base::OnceClosure cancel_callback),
              (override));
  MOCK_METHOD(void,
              CloseProgressUi,
              (bool show_confirmation_before_closing),
              (override));
  MOCK_METHOD(void,
              ShowAutofillErrorUi,
              (AutofillErrorDialogContext context),
              (override));
};
}  // namespace

class BnplManagerTest : public Test,
                        public WithTestAutofillClientDriverManager<
                            TestAutofillClient,
                            TestAutofillDriver,
                            NiceMock<MockBrowserAutofillManager>,
                            TestPaymentsAutofillClientMock> {
 public:
  using GetDetailsForUpdateBnplPaymentInstrumentRequestDetails::
      GetDetailsForUpdateBnplPaymentInstrumentType::kGetDetailsForAcceptTos;
  using UpdateBnplPaymentInstrumentRequestDetails::
      UpdateBnplPaymentInstrumentType::kAcceptTos;

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
  const int64_t kAmount = 1'000'000;

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
    InitAutofillClient();
    autofill_client().set_app_locale(kAppLocale);
    autofill_client().set_last_committed_primary_main_frame_url(kDomain);
    autofill_client().GetPersonalDataManager().set_payments_data_manager(
        std::make_unique<TestPaymentsDataManager>());
    autofill_client()
        .GetPersonalDataManager()
        .test_payments_data_manager()
        .SetPrefService(autofill_client().GetPrefs());
    autofill_client()
        .GetPersonalDataManager()
        .payments_data_manager()
        .SetSyncingForTest(true);
    autofill_client()
        .GetPersonalDataManager()
        .test_payments_data_manager()
        .SetPaymentsCustomerData(std::make_unique<PaymentsCustomerData>(
            base::NumberToString(kBillingCustomerNumber)));

    std::unique_ptr<PaymentsNetworkInterfaceMock> payments_network_interface =
        std::make_unique<PaymentsNetworkInterfaceMock>();
    payments_network_interface_ = payments_network_interface.get();

    autofill_client().set_payments_autofill_client(
        std::make_unique<TestPaymentsAutofillClientMock>(&autofill_client()));
    payments_autofill_client().set_payments_network_interface(
        std::move(payments_network_interface));

    payments_autofill_client().SetAutofillPaymentMethodsEnabled(true);

    payments_autofill_client().set_bnpl_ui_delegate(
        std::make_unique<NiceMock<MockBnplUiDelegate>>());

    CreateAutofillDriver();

    credit_card_form_event_logger_ =
        std::make_unique<NiceMock<MockCreditCardFormEventLogger>>(
            &autofill_manager());

    ON_CALL(autofill_manager(), GetCreditCardFormEventLogger())
        .WillByDefault(ReturnRef(*credit_card_form_event_logger_));

    mock_amount_extraction_manager_ =
        std::make_unique<testing::NiceMock<MockAmountExtractionManager>>(
            &autofill_manager());

    ON_CALL(autofill_manager(), GetAmountExtractionManager())
        .WillByDefault(ReturnRef(*mock_amount_extraction_manager_));

    bnpl_manager_ = std::make_unique<BnplManager>(&autofill_manager());

    ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
                autofill_client().GetAutofillOptimizationGuideDecider()),
            IsUrlEligibleForBnplIssuer)
        .WillByDefault(Return(true));
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Sets up the PersonalDataManager with a unlinked bnpl issuer.
  void SetUpUnlinkedBnplIssuer(int64_t price_lower_bound_in_micros,
                               int64_t price_higher_bound_in_micros,
                               IssuerId issuer_id) {
    std::vector<BnplIssuer::EligiblePriceRange> eligible_price_ranges;
    eligible_price_ranges.emplace_back(kCurrency, price_lower_bound_in_micros,
                                       price_higher_bound_in_micros);
    test_api(autofill_client().GetPersonalDataManager().payments_data_manager())
        .AddBnplIssuer(BnplIssuer(std::nullopt, issuer_id,
                                  std::move(eligible_price_ranges)));
  }

  // Sets up the PersonalDataManager with a linked bnpl issuer.
  void SetUpLinkedBnplIssuer(int64_t price_lower_bound_in_micros,
                             int64_t price_higher_bound_in_micros,
                             IssuerId issuer_id,
                             const int64_t instrument_id) {
    std::vector<BnplIssuer::EligiblePriceRange> eligible_price_ranges;
    eligible_price_ranges.emplace_back(kCurrency, price_lower_bound_in_micros,
                                       price_higher_bound_in_micros);

    test_api(autofill_client().GetPersonalDataManager().payments_data_manager())
        .AddBnplIssuer(BnplIssuer(instrument_id, issuer_id,
                                  std::move(eligible_price_ranges)));
  }

  void TriggerBnplUpdateSuggestionsFlow(bool expect_suggestions_are_updated,
                                        std::optional<int64_t> extracted_amount,
                                        bool timeout_reached = false) {
    std::vector<Suggestion> suggestions = {
        Suggestion(SuggestionType::kCreditCardEntry),
        Suggestion(SuggestionType::kManageCreditCard)};
    base::MockCallback<UpdateSuggestionsCallback> callback;
    expect_suggestions_are_updated ? EXPECT_CALL(callback, Run)
                                   : EXPECT_CALL(callback, Run).Times(0);

    bnpl_manager_->NotifyOfSuggestionGeneration(
        AutofillSuggestionTriggerSource::kUnspecified);
    bnpl_manager_->OnAmountExtractionReturned(extracted_amount,
                                              timeout_reached);
    bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
  }

  LegalMessageLines GetExpectedLegalMessageLines() {
    return {TestLegalMessageLine(base::UTF16ToUTF8(kLegalMessage))};
  }

  void OnIssuerSelected(const BnplIssuer& selected_issuer) {
    bnpl_manager_->OnIssuerSelected(selected_issuer);
  }

  bool ShouldCloseViewBeforeSwitching() {
#if BUILDFLAG(IS_ANDROID)
    return false;
#else
    return true;
#endif
  }

  MockBnplUiDelegate& GetBnplUiDelegate() {
    return *static_cast<MockBnplUiDelegate*>(
        payments_autofill_client().GetBnplUiDelegate());
  }

  void TearDown() override {
    credit_card_form_event_logger_->OnDestroyed();
    credit_card_form_event_logger_.reset();
    autofill_client()
        .GetPersonalDataManager()
        .test_payments_data_manager()
        .ClearBnplIssuers();
    histogram_tester_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockCreditCardFormEventLogger> credit_card_form_event_logger_;
  std::unique_ptr<BnplManager> bnpl_manager_;
  std::unique_ptr<MockAmountExtractionManager> mock_amount_extraction_manager_;
  raw_ptr<PaymentsNetworkInterfaceMock> payments_network_interface_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
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
  EXPECT_EQ(autofill_client().GetAppLocale(),
            test_api(*bnpl_manager_).GetOngoingFlowState()->app_locale);
  EXPECT_EQ(
      GetBillingCustomerId(payments_autofill_client().GetPaymentsDataManager()),
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
  int64_t final_checkout_amount = 1000000;
  autofill_client().set_app_locale("en_GB");
  bnpl_manager_->OnDidAcceptBnplSuggestion(final_checkout_amount,
                                           base::DoNothing());

  EXPECT_EQ(
      final_checkout_amount,
      test_api(*bnpl_manager_).GetOngoingFlowState()->final_checkout_amount);
  EXPECT_EQ(autofill_client().GetAppLocale(),
            test_api(*bnpl_manager_).GetOngoingFlowState()->app_locale);
  EXPECT_EQ(
      GetBillingCustomerId(payments_autofill_client().GetPaymentsDataManager()),
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
                                      autofill_client().GetAppLocale(),
                                      GetBillingCustomerId(
                                          payments_autofill_client()
                                              .GetPaymentsDataManager()),
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

  payments_autofill_client().set_risk_data_loaded(false);

  EXPECT_CALL(
      *payments_network_interface_,
      CreateBnplPaymentInstrument(/*request_details=*/
                                  FieldsAre(
                                      autofill_client().GetAppLocale(),
                                      GetBillingCustomerId(
                                          payments_autofill_client()
                                              .GetPaymentsDataManager()),
                                      autofill::ConvertToBnplIssuerIdString(
                                          test_issuer.issuer_id()),
                                      test_context_token, risk_data),
                                  /*callback=*/_));
  test_api(*bnpl_manager_).OnTosDialogAccepted();

  EXPECT_FALSE(ongoing_flow_state->risk_data.empty());

  // Since risk data was cached, it was directly used, thus loading risk data
  // was skipped.
  EXPECT_FALSE(payments_autofill_client().risk_data_loaded());
}

// Tests that the user accepting the ToS dialog for a linked issuer where ToS
// acceptance is required triggers an UpdatePaymentInstrument request with the
// loaded risk data, if it is present.
TEST_F(BnplManagerTest,
       TosDialogAccepted_PrefetchedRiskDataLoaded_TosAcceptanceRequired) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(/*final_checkout_amount=*/kAmount,
                                           base::DoNothing());
  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  const std::string test_context_token = "test_context_token";
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer(
      IssuerId::kBnplKlarna,
      DenseSet<PaymentInstrument::ActionRequired>{
          PaymentInstrument::ActionRequired::kAcceptTos});
  ongoing_flow_state->context_token = test_context_token;
  ongoing_flow_state->issuer = issuer;

  ASSERT_FALSE(ongoing_flow_state->risk_data.empty());

  payments_autofill_client().set_risk_data_loaded(false);

  EXPECT_CALL(
      *payments_network_interface_,
      UpdateBnplPaymentInstrument(/*request_details=*/
                                  FieldsAre(
                                      autofill_client().GetAppLocale(),
                                      GetBillingCustomerId(
                                          payments_autofill_client()
                                              .GetPaymentsDataManager()),
                                      autofill::ConvertToBnplIssuerIdString(
                                          issuer.issuer_id()),
                                      /*instrument_id=*/12345,
                                      test_context_token, /*risk_data=*/_,
                                      /*type=*/kAcceptTos),
                                  /*callback=*/_));

  test_api(*bnpl_manager_).OnTosDialogAccepted();

  EXPECT_FALSE(ongoing_flow_state->risk_data.empty());

  // Since risk data was cached, it was directly used, thus loading risk data
  // was skipped.
  EXPECT_FALSE(payments_autofill_client().risk_data_loaded());
}

// Tests that the the user accepting the ToS dialog for a linked issuer where
// ToS acceptance is required triggers an UpdatePaymentInstrument request and
// loads risk data after ToS dialog acceptance if it was not already loaded.
TEST_F(BnplManagerTest,
       TosDialogAccepted_PrefetchedRiskDataNotLoaded_TosAcceptanceRequired) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(/*final_checkout_amount=*/1000000,
                                           base::DoNothing());
  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  const std::string test_context_token = "test_context_token";
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer(
      IssuerId::kBnplKlarna,
      DenseSet<PaymentInstrument::ActionRequired>{
          PaymentInstrument::ActionRequired::kAcceptTos});
  ongoing_flow_state->context_token = test_context_token;
  ongoing_flow_state->issuer = issuer;
  ongoing_flow_state->risk_data.clear();

  ASSERT_TRUE(ongoing_flow_state->risk_data.empty());

  EXPECT_CALL(
      *payments_network_interface_,
      UpdateBnplPaymentInstrument(/*request_details=*/
                                  FieldsAre(
                                      autofill_client().GetAppLocale(),
                                      GetBillingCustomerId(
                                          payments_autofill_client()
                                              .GetPaymentsDataManager()),
                                      autofill::ConvertToBnplIssuerIdString(
                                          issuer.issuer_id()),
                                      issuer.payment_instrument()
                                          ->instrument_id(),
                                      test_context_token,
                                      /*risk_data=*/_, /*type=*/kAcceptTos),
                                  /*callback=*/_));
  test_api(*bnpl_manager_).OnTosDialogAccepted();

  EXPECT_FALSE(ongoing_flow_state->risk_data.empty());
  EXPECT_TRUE(payments_autofill_client().risk_data_loaded());
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
      .WillOnce(SaveArg<0>(&fetched_vcn));

  EXPECT_NE(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);

  test_api(*bnpl_manager_).FetchVcnDetails(kPopupUrl);

  EXPECT_CALL(GetBnplUiDelegate(),
              CloseProgressUi(/*show_confirmation_before_closing=*/true));

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
  EXPECT_EQ(fetched_vcn.server_id(),
            base::NumberToString(issuer.payment_instrument()->instrument_id()));
  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that OnVcnDetailsFetched shows an error UI when there is a
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

  EXPECT_CALL(GetBnplUiDelegate(),
              CloseProgressUi(/*show_confirmation_before_closing=*/false));
  EXPECT_CALL(GetBnplUiDelegate(),
              ShowAutofillErrorUi(
                  AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                      /*is_permanent_error=*/true)));

  test_api(*bnpl_manager_)
      .OnVcnDetailsFetched(PaymentsAutofillClient::PaymentsRpcResult::
                               kVcnRetrievalPermanentFailure,
                           response_details);

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
          /*callback=*/_));

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

  payments_autofill_client().set_risk_data_loaded(false);

  EXPECT_CALL(
      *payments_network_interface_,
      GetBnplPaymentInstrumentForFetchingUrl(
          /*request_details=*/
          FieldsAre(kBillingCustomerNumber,
                    base::NumberToString(
                        linked_issuer.payment_instrument()->instrument_id()),
                    kRiskData, url::Origin::Create(GURL(kDomain)).GetURL(),
                    kAmount, kCurrency),
          /*callback=*/_));

  OnIssuerSelected(linked_issuer);

  EXPECT_EQ(ongoing_flow_state->issuer, linked_issuer);
  EXPECT_EQ(ongoing_flow_state->instrument_id,
            base::NumberToString(
                linked_issuer.payment_instrument()->instrument_id()));
  EXPECT_EQ(ongoing_flow_state->risk_data, kRiskData);

  // Since risk data was cached, it was directly used, thus loading risk data
  // was skipped.
  EXPECT_FALSE(payments_autofill_client().risk_data_loaded());
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
                  payments_autofill_client().GetPaymentsWindowManager()),
              InitBnplFlow(FieldsAre(linked_issuer.issuer_id(), kRedirectUrl,
                                     response.success_url_prefix,
                                     response.failure_url_prefix,
                                     /*completion_callback=*/_)));
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

  EXPECT_CALL(GetBnplUiDelegate(),
              ShowAutofillErrorUi(
                  AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                      /*is_permanent_error=*/false)));

  OnIssuerSelected(linked_issuer);

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

  EXPECT_CALL(GetBnplUiDelegate(),
              ShowAutofillErrorUi(
                  AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                      /*is_permanent_error=*/true)));

  OnIssuerSelected(linked_issuer);

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
      payments_autofill_client().GetPaymentsWindowManager());

  EXPECT_CALL(payments_window_manager, InitBnplFlow)
      .WillOnce([&](PaymentsWindowManager::BnplContext bnpl_context) {
        std::move(bnpl_context.completion_callback)
            .Run(PaymentsWindowManager::BnplFlowResult::kSuccess, kPopupUrl);
      });

  GetBnplPaymentInstrumentForFetchingVcnRequestDetails request_details;
  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingVcn)
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
      payments_autofill_client().GetPaymentsWindowManager());

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
      payments_autofill_client().GetPaymentsWindowManager());

  EXPECT_CALL(payments_window_manager, InitBnplFlow)
      .WillOnce([&](PaymentsWindowManager::BnplContext bnpl_context) {
        std::move(bnpl_context.completion_callback)
            .Run(PaymentsWindowManager::BnplFlowResult::kFailure, kPopupUrl);
      });

  EXPECT_CALL(GetBnplUiDelegate(),
              ShowAutofillErrorUi(
                  AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                      /*is_permanent_error=*/false)));

  OnIssuerSelected(linked_issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that FetchVcnDetails will display an autofill progress UI.
TEST_F(BnplManagerTest, FetchVcnDetails_ShowProgressUi) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(1'000'000, base::DoNothing());
  test_api(*bnpl_manager_)
      .PopulateManagerWithUserAndBnplIssuerDetails(
          kBillingCustomerNumber, kInstrumentId, kRiskData, kContextToken,
          kRedirectUrl, test::GetTestLinkedBnplIssuer());

  EXPECT_CALL(GetBnplUiDelegate(),
              ShowProgressUi(
                  AutofillProgressDialogType::kBnplFetchVcnProgressDialog, _));

  test_api(*bnpl_manager_).FetchVcnDetails(kPopupUrl);
}

// Tests that calling Reset while fetching VCN details will reset the status of
// BnplManager.
TEST_F(BnplManagerTest, FetchVcnDetails_Reset) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(1'000'000, base::DoNothing());
  test_api(*bnpl_manager_)
      .PopulateManagerWithUserAndBnplIssuerDetails(
          kBillingCustomerNumber, kInstrumentId, kRiskData, kContextToken,
          kRedirectUrl, test::GetTestLinkedBnplIssuer());

  EXPECT_NE(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);

  EXPECT_CALL(GetBnplUiDelegate(),
              ShowProgressUi(
                  AutofillProgressDialogType::kBnplFetchVcnProgressDialog, _));

  test_api(*bnpl_manager_).FetchVcnDetails(kPopupUrl);

  EXPECT_NE(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);

  test_api(*bnpl_manager_).Reset();

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that `OnDidGetLegalMessageFromServer` set the BNPL manager state if the
// request has completed successfully, and shows the ToS UI. This test also
// ensures the ToS/progress UI is closed after receiving a redirect URL for
// an unlinked issuer.
TEST_F(BnplManagerTest,
       OnDidGetLegalMessageFromServer_ClosesTosAfterRedirectUrlReceived) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(1'000'000, base::DoNothing());
  BnplIssuer unlinked_issuer = test::GetTestUnlinkedBnplIssuer();

  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForCreateBnplPaymentInstrument)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, kContextToken,
          GetExpectedLegalMessageLines()));

  BnplTosModel bnpl_tos_model;
  EXPECT_CALL(GetBnplUiDelegate(), ShowBnplTosUi)
      .WillOnce(SaveArg<0>(&bnpl_tos_model));
  OnIssuerSelected(unlinked_issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->context_token,
            kContextToken);

  const LegalMessageLines& legal_message_lines =
      bnpl_tos_model.legal_message_lines;
  ASSERT_FALSE(legal_message_lines.empty());
  EXPECT_EQ(legal_message_lines[0].text(), kLegalMessage);

  EXPECT_EQ(bnpl_tos_model.issuer, unlinked_issuer);

  EXPECT_CALL(GetBnplUiDelegate(), RemoveBnplTosOrProgressUi());

  test_api(*bnpl_manager_)
      .OnRedirectUrlFetched(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                            BnplFetchUrlResponseDetails());
}

// Tests that cancelling the ToS UI resets and ends the flow.
TEST_F(BnplManagerTest,
       OnDidGetLegalMessageFromServer_TosCancellationResetsFlow) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(1'000'000, base::DoNothing());
  BnplIssuer unlinked_issuer = test::GetTestUnlinkedBnplIssuer();

  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForCreateBnplPaymentInstrument)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, kContextToken,
          GetExpectedLegalMessageLines()));

  // Cancel the ToS UI by running the cancel callback (2nd param).
  EXPECT_CALL(GetBnplUiDelegate(), ShowBnplTosUi)
      .WillOnce(base::test::RunOnceCallback<2>());

  EXPECT_NE(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);

  OnIssuerSelected(unlinked_issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that `OnIssuerSelected()` correctly sets the instrument_id for an
// externally linked issuer before proceeding with the flow.
TEST_F(BnplManagerTest,
       OnIssuerSelected_SetsInstrumentIdForExternallyLinkedIssuer) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  BnplIssuer externally_linked_issuer = test::GetTestLinkedBnplIssuer(
      BnplIssuer::IssuerId::kBnplKlarna,
      /*action_required=*/autofill::DenseSet(
          {autofill::PaymentInstrument::ActionRequired::kAcceptTos}));

  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForUpdateBnplPaymentInstrument);

  OnIssuerSelected(externally_linked_issuer);

  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  EXPECT_EQ(ongoing_flow_state->issuer, externally_linked_issuer);
  EXPECT_EQ(
      ongoing_flow_state->instrument_id,
      base::NumberToString(
          externally_linked_issuer.payment_instrument()->instrument_id()));
}

// Tests that `OnDidGetLegalMessageFromServer` shows an error when there is a
// PaymentsRpcResult error.
TEST_F(BnplManagerTest, OnDidGetLegalMessageFromServer_RpcError) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(1'000'000, base::DoNothing());
  BnplIssuer unlinked_issuer = test::GetTestUnlinkedBnplIssuer();

  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForCreateBnplPaymentInstrument)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kTryAgainFailure,
          kContextToken, GetExpectedLegalMessageLines()));

  EXPECT_CALL(GetBnplUiDelegate(),
              ShowAutofillErrorUi(
                  AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                      /*is_permanent_error=*/false)));

  OnIssuerSelected(unlinked_issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that `OnDidAcceptBnplSuggestion()` calls `ShowSelectBnplIssuerUi()` on
// the UI delegate.
TEST_F(BnplManagerTest, OnDidAcceptBnplSuggestion_ShowSelectBnplIssuerUi) {
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi);

  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
}

// Tests that the BNPL flow will be reset if the user cancels the select issuer
// UI.
TEST_F(BnplManagerTest, ShowSelectBnplIssuerUi_UserCancelled) {
  InSequence s;
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi)
      .WillOnce(base::test::RunOnceCallback<3>());

  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that `OnDidGetLegalMessageFromServer` will remove the issuer selection
// UI or progress throbber UI.
TEST_F(BnplManagerTest,
       OnDidGetLegalMessageFromServer_RemoveSelectBnplIssuerOrProgressUi) {
  const BnplIssuer unlinked_issuer = test::GetTestUnlinkedBnplIssuer();

  InSequence s;
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi)
      .WillOnce(base::test::RunOnceCallback<2>(unlinked_issuer));
  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForCreateBnplPaymentInstrument)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, kContextToken,
          GetExpectedLegalMessageLines()));
  EXPECT_CALL(GetBnplUiDelegate(), RemoveSelectBnplIssuerOrProgressUi())
      .Times(ShouldCloseViewBeforeSwitching() ? 1 : 0);

  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->issuer,
            unlinked_issuer);
}

// Tests that `GetDetailsForUpdateBnplPaymentInstrument` calls the payments
// network interface with the request details filled out correctly.
TEST_F(BnplManagerTest, GetDetailsForUpdateBnplPaymentInstrument_Success) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());

  BnplIssuer issuer = test::GetTestLinkedBnplIssuer(
      IssuerId::kBnplKlarna, {PaymentInstrument::ActionRequired::kAcceptTos});
  test_api(*bnpl_manager_).GetOngoingFlowState()->issuer = issuer;

  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForUpdateBnplPaymentInstrument(
                  /*request_details=*/
                  FieldsAre(kAppLocale, kBillingCustomerNumber,
                            /*client_behavior_signals=*/IsEmpty(),
                            issuer.payment_instrument()->instrument_id(),
                            /*type=*/kGetDetailsForAcceptTos,
                            /*issuer_id=*/kBnplKlarnaIssuerId),
                  /*callback=*/_));

  test_api(*bnpl_manager_).GetDetailsForUpdateBnplPaymentInstrument();
}

// Tests that `UpdateBnplPaymentInstrument` calls the payments network interface
// with the request details filled out correctly.
TEST_F(BnplManagerTest, UpdateBnplPaymentInstrument_Success) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());

  BnplIssuer issuer = test::GetTestLinkedBnplIssuer();
  test_api(*bnpl_manager_).GetOngoingFlowState()->context_token = kContextToken;
  test_api(*bnpl_manager_).GetOngoingFlowState()->risk_data = kRiskData;
  test_api(*bnpl_manager_).GetOngoingFlowState()->issuer = issuer;

  EXPECT_CALL(
      *payments_network_interface_,
      UpdateBnplPaymentInstrument(
          FieldsAre(kAppLocale, kBillingCustomerNumber,
                    autofill::ConvertToBnplIssuerIdString(issuer.issuer_id()),
                    issuer.payment_instrument()->instrument_id(), kContextToken,
                    kRiskData, /*type=*/kAcceptTos),
          /*callback=*/_));

  test_api(*bnpl_manager_).UpdateBnplPaymentInstrument();
}

// Tests that a successful `UpdateBnplPaymentInstrument` response results in a
// call to fetch the redirect URL.
TEST_F(BnplManagerTest, OnBnplPaymentInstrumentUpdated_Success) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  test_api(*bnpl_manager_).GetOngoingFlowState()->issuer =
      test::GetTestLinkedBnplIssuer();

  EXPECT_CALL(*payments_network_interface_, UpdateBnplPaymentInstrument)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess));

  // Successful update should trigger fetching the redirect URL.
  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingUrl);

  test_api(*bnpl_manager_).UpdateBnplPaymentInstrument();
}

// Tests that a failed `UpdateBnplPaymentInstrument` response shows an error
// UI and resets the flow.
TEST_F(BnplManagerTest, OnBnplPaymentInstrumentUpdated_Failure) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  test_api(*bnpl_manager_).GetOngoingFlowState()->issuer =
      test::GetTestLinkedBnplIssuer();

  EXPECT_CALL(*payments_network_interface_, UpdateBnplPaymentInstrument)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure));
  EXPECT_CALL(GetBnplUiDelegate(), RemoveBnplTosOrProgressUi())
      .Times(ShouldCloseViewBeforeSwitching() ? 1 : 0);
  EXPECT_CALL(GetBnplUiDelegate(),
              ShowAutofillErrorUi(
                  AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                      /*is_permanent_error=*/false)));

  test_api(*bnpl_manager_).UpdateBnplPaymentInstrument();

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that `OnRedirectUrlFetched` will remove the issuer selection UI or
// progress throbber UI
TEST_F(BnplManagerTest,
       OnRedirectUrlFetched_LinkedIssuer_RemoveSelectBnplIssuerOrProgressUi) {
  BnplFetchUrlResponseDetails response;
  response.redirect_url = kRedirectUrl;
  response.success_url_prefix = GURL("success");
  response.failure_url_prefix = GURL("failure");
  response.context_token = kContextToken;

  InSequence s;
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi)
      .WillOnce(
          base::test::RunOnceCallback<2>(test::GetTestLinkedBnplIssuer()));
  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingUrl)
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, response));
  EXPECT_CALL(GetBnplUiDelegate(), RemoveSelectBnplIssuerOrProgressUi())
      .Times(ShouldCloseViewBeforeSwitching() ? 1 : 0);

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
  EXPECT_CALL(callback, Run);

  bnpl_manager_->NotifyOfSuggestionGeneration(
      AutofillSuggestionTriggerSource::kUnspecified);
  bnpl_manager_->OnSuggestionsShown(suggestions, callback.Get());
  bnpl_manager_->OnAmountExtractionReturned(1'234'560'000ULL,
                                            /*timeout_reached=*/false);
}

TEST_F(BnplManagerTest, ValidAmountReturnedInTimeUpdateUi) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());
  int64_t test_amount = 50'000'000;
  SetUpUnlinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/10'000'000,
      /*price_higher_bound_in_micros=*/1'000'000'000, IssuerId::kBnplAfterpay);

  EXPECT_CALL(GetBnplUiDelegate(), UpdateBnplIssuerDialogUi);

  bnpl_manager_->OnAmountExtractionReturnedFromAi(test_amount,
                                                  /*timeout_reached=*/false);
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

// Tests that BnplSuggestionUnavailableReason will be logged once if the amount
// extraction engine fails to pass in a valid value.
TEST_F(
    BnplManagerTest,
    AddBnplSuggestion_NoAmountPassedIn_BnplSuggestionUnavailableReasonLogged) {
  // Add one linked issuer to payments data manager.
  SetUpLinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/40,
      /*price_higher_bound_in_micros=*/1000, IssuerId::kBnplAffirm,
      /*instrument_id=*/1234);

  TriggerBnplUpdateSuggestionsFlow(/*expect_suggestions_are_updated=*/false,
                                   /*extracted_amount=*/std::nullopt);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kAmountExtractionFailure,
      1);

  // Test that BnplSuggestionUnavailableReason is logged only once even if BNPL
  // flow is triggered and unavailable more than once on the same page.
  TriggerBnplUpdateSuggestionsFlow(/*expect_suggestions_are_updated=*/false,
                                   /*extracted_amount=*/std::nullopt);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kAmountExtractionFailure,
      1);
}

// Tests that BnplSuggestionUnavailableReason will not be logged if BNPL feature
// flag is disabled and the amount extraction engine fails to pass in a valid
// value.
TEST_F(
    BnplManagerTest,
    AddBnplSuggestion_NoAmountPassedIn_BnplSuggestionUnavailableReasonNotLogged_BnplDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLater});

  // Add one linked issuer to payments data manager.
  SetUpLinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/40,
      /*price_higher_bound_in_micros=*/1000, IssuerId::kBnplAffirm,
      /*instrument_id=*/1234);

  TriggerBnplUpdateSuggestionsFlow(/*expect_suggestions_are_updated=*/false,
                                   /*extracted_amount=*/std::nullopt);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kAmountExtractionFailure,
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

// Tests that BnplSuggestionUnavailableReason will be logged once if the
// extracted amount is too high and is not supported by available BNPL issuers.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_AmountTooHigh_BnplSuggestionUnavailableReasonLogged) {
  // Add one linked issuer to payments data manager.
  SetUpLinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/40,
      /*price_higher_bound_in_micros=*/1000, IssuerId::kBnplAffirm,
      /*instrument_id=*/1234);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/30'000'000ULL);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kCheckoutAmountNotSupported,
      1);

  // Test that BnplSuggestionUnavailableReason is logged only once even if BNPL
  // flow is triggered and unavailable more than once on the same page.
  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/30'000'000ULL);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kCheckoutAmountNotSupported,
      1);
}

// Tests that BnplSuggestionUnavailableReason will be logged once if the
// extracted amount is too low and is not supported by available BNPL issuers.
TEST_F(BnplManagerTest,
       AddBnplSuggestion_AmountTooLow_BnplSuggestionUnavailableReasonLogged) {
  // Add one linked issuer to payments data manager.
  SetUpLinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/40,
      /*price_higher_bound_in_micros=*/1000, IssuerId::kBnplAffirm,
      /*instrument_id=*/1234);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/20ULL);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kCheckoutAmountNotSupported,
      1);

  // Test that BnplSuggestionUnavailableReason is logged only once even if BNPL
  // flow is triggered and unavailable more than once on the same page.
  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/20ULL);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kCheckoutAmountNotSupported,
      1);
}

// Tests that BnplSuggestionUnavailableReason will be logged once if the amount
// extraction engine times out.
TEST_F(
    BnplManagerTest,
    AddBnplSuggestion_AmountExtractionTimeout_BnplSuggestionUnavailableReasonLogged) {
  // Add one linked issuer to payments data manager.
  SetUpLinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/40,
      /*price_higher_bound_in_micros=*/1000, IssuerId::kBnplAffirm,
      /*instrument_id=*/1234);

  TriggerBnplUpdateSuggestionsFlow(/*expect_suggestions_are_updated=*/false,
                                   /*extracted_amount=*/std::nullopt,
                                   /*timeout_reached=*/true);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kAmountExtractionTimeout,
      1);

  // Test that BnplSuggestionUnavailableReason is logged only once even if BNPL
  // flow is triggered and unavailable more than once on the same page.
  TriggerBnplUpdateSuggestionsFlow(/*expect_suggestions_are_updated=*/false,
                                   /*extracted_amount=*/std::nullopt,
                                   /*timeout_reached=*/true);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kAmountExtractionTimeout,
      1);
}

// Tests that BnplSuggestionUnavailableReason will not be logged if BNPL feature
// flag is disabled and the extracted amount is not supported by available
// BNPL issuers.
TEST_F(
    BnplManagerTest,
    AddBnplSuggestion_AmountNotSupported_BnplSuggestionUnavailableReasonNotLogged_BnplDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterSyncing},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLater});

  // Add one linked issuer to payments data manager.
  SetUpLinkedBnplIssuer(
      /*price_lower_bound_in_micros=*/40,
      /*price_higher_bound_in_micros=*/1000, IssuerId::kBnplAffirm,
      /*instrument_id=*/1234);

  TriggerBnplUpdateSuggestionsFlow(
      /*expect_suggestions_are_updated=*/false,
      /*extracted_amount=*/30'000'000ULL);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
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
  prefs::SetAutofillBnplEnabled(autofill_client().GetPrefs(), false);

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
  bnpl_manager_->OnAmountExtractionReturned(1'234'560'000ULL,
                                            /*timeout_reached=*/false);
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
  bnpl_manager_->OnAmountExtractionReturned(1'234'560'000ULL,
                                            /*timeout_reached=*/false);
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
                                ongoing_flow_state->issuer->issuer_id()),
                            kContextToken, kRiskData),
                  _))
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kSuccess, kInstrumentId));

  EXPECT_CALL(*payments_network_interface_,
              GetBnplPaymentInstrumentForFetchingUrl(
                  FieldsAre(kBillingCustomerNumber, kInstrumentId, kRiskData,
                            url::Origin::Create(GURL(kDomain)).GetURL(),
                            kAmount, kCurrency),
                  _));

  test_api(*bnpl_manager_).CreateBnplPaymentInstrument();

  EXPECT_EQ(ongoing_flow_state->instrument_id, kInstrumentId);
}

// Tests that when CreateBnplPaymentInstrument fails with an error the error
// UI is shown and the flow is reset.
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
                                ongoing_flow_state->issuer->issuer_id()),
                            kContextToken, kRiskData),
                  _))
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure, ""));

  EXPECT_CALL(GetBnplUiDelegate(), RemoveBnplTosOrProgressUi())
      .Times(ShouldCloseViewBeforeSwitching() ? 1 : 0);
  EXPECT_CALL(GetBnplUiDelegate(),
              ShowAutofillErrorUi(
                  AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                      /*is_permanent_error=*/false)));

  test_api(*bnpl_manager_).CreateBnplPaymentInstrument();

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

// Tests that when UpdateBnplPaymentInstrument fails with an error the error
// UI is shown and the flow is reset.
TEST_F(BnplManagerTest, UpdateBnplPaymentInstrument_Failure) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  auto* ongoing_flow_state = test_api(*bnpl_manager_).GetOngoingFlowState();
  ongoing_flow_state->app_locale = kAppLocale;
  ongoing_flow_state->billing_customer_number = kBillingCustomerNumber;
  ongoing_flow_state->context_token = kContextToken;
  ongoing_flow_state->issuer = test::GetTestLinkedBnplIssuer(
      IssuerId::kBnplKlarna,
      DenseSet<PaymentInstrument::ActionRequired>{
          PaymentInstrument::ActionRequired::kAcceptTos});
  ongoing_flow_state->risk_data = kRiskData;

  EXPECT_CALL(
      *payments_network_interface_,
      UpdateBnplPaymentInstrument(
          FieldsAre(
              kAppLocale, kBillingCustomerNumber,
              autofill::ConvertToBnplIssuerIdString(
                  ongoing_flow_state->issuer->issuer_id()),
              ongoing_flow_state->issuer->payment_instrument()->instrument_id(),
              kContextToken, kRiskData,
              /*type=*/kAcceptTos),
          _))
      .WillOnce(base::test::RunOnceCallback<1>(
          PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure));

  EXPECT_CALL(GetBnplUiDelegate(), RemoveBnplTosOrProgressUi())
      .Times(ShouldCloseViewBeforeSwitching() ? 1 : 0);
  EXPECT_CALL(GetBnplUiDelegate(),
              ShowAutofillErrorUi(
                  AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                      /*is_permanent_error=*/false)));

  test_api(*bnpl_manager_).UpdateBnplPaymentInstrument();

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
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer(IssuerId::kBnplAfterpay, _))
      .WillByDefault(Return(false));
  ON_CALL(
      *static_cast<MockAutofillOptimizationGuideDecider*>(
          autofill_client().GetAutofillOptimizationGuideDecider()),
      IsUrlEligibleForBnplIssuer(
          Matcher<IssuerId>(AnyOf(IssuerId::kBnplAffirm, IssuerId::kBnplZip)),
          _))
      .WillByDefault(Return(true));

  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi)
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
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer(
              Matcher<IssuerId>(
                  AnyOf(IssuerId::kBnplAffirm, IssuerId::kBnplAfterpay)),
              _))
      .WillByDefault(Return(false));
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer(IssuerId::kBnplZip, _))
      .WillByDefault(Return(true));

  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi)
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
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(true));

  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi)
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

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(false));

  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi)
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

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(true));

  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi)
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

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(Return(true));

  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi)
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
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  histogram_tester_->ExpectUniqueSample(
      "Autofill.FormEvents.CreditCard.Bnpl",
      /*sample=*/autofill_metrics::BnplFormEvent::kBnplSuggestionAccepted,
      /*expected_bucket_count=*/1);

  // Test that `kBnplSuggestionAccepted` is logged only once even if
  // `OnDidAcceptBnplSuggestion()` is called more than once on the same page.
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());
  histogram_tester_->ExpectUniqueSample(
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
  bnpl_manager_->OnAmountExtractionReturned(50'000'000ULL,
                                            /*timeout_reached=*/false);
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
  bnpl_manager_->OnAmountExtractionReturned(1'234'560'000ULL,
                                            /*timeout_reached=*/false);
}

TEST_F(BnplManagerTest, IsBnplIssuerSupported) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableBuyNowPayLaterForKlarna);

  EXPECT_TRUE(BnplManager::IsBnplIssuerSupported(kBnplAffirmIssuerId));
  EXPECT_TRUE(BnplManager::IsBnplIssuerSupported(kBnplZipIssuerId));
  EXPECT_TRUE(BnplManager::IsBnplIssuerSupported(kBnplKlarnaIssuerId));
}

TEST_F(BnplManagerTest, IsBnplIssuerSupported_KlarnaDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableBuyNowPayLaterForKlarna);

  EXPECT_TRUE(BnplManager::IsBnplIssuerSupported(kBnplAffirmIssuerId));
  EXPECT_TRUE(BnplManager::IsBnplIssuerSupported(kBnplZipIssuerId));
  EXPECT_FALSE(BnplManager::IsBnplIssuerSupported(kBnplKlarnaIssuerId));
}

TEST_F(BnplManagerTest,
       OnDidAcceptBnplSuggestion_AiBasedAmountExtractionPrefTurnedOn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableAiBasedAmountExtraction);
  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAmountExtractionAiTermsSeen, true);

  EXPECT_CALL(*mock_amount_extraction_manager_,
              TriggerCheckoutAmountExtractionWithAi());
  EXPECT_CALL(GetBnplUiDelegate(),
              ShowSelectBnplIssuerUi(testing::IsEmpty(), Eq(kAppLocale), _, _,
                                     /*has_seen_ai_terms=*/true));

  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());
}

TEST_F(BnplManagerTest,
       OnDidAcceptBnplSuggestion_AiBasedAmountExtractionPrefTurnedOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableAiBasedAmountExtraction);

  EXPECT_CALL(*mock_amount_extraction_manager_,
              TriggerCheckoutAmountExtractionWithAi())
      .Times(0);
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi);

  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());
}

TEST_F(BnplManagerTest,
       OnDidAcceptBnplSuggestion_AiBasedAmountExtractionFeatureDisabled) {
  EXPECT_CALL(*mock_amount_extraction_manager_,
              TriggerCheckoutAmountExtractionWithAi())
      .Times(0);
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi);

  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());
}

// Tests that `OnDidAcceptBnplSuggestion` triggers amount extraction if the
// user has already seen the terms and the feature is enabled.
TEST_F(BnplManagerTest,
       OnDidAcceptBnplSuggestion_TriggersExtractionIfTermsSeen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableAiBasedAmountExtraction);
  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAmountExtractionAiTermsSeen, true);

  InSequence s;
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi);
  EXPECT_CALL(*mock_amount_extraction_manager_,
              TriggerCheckoutAmountExtractionWithAi());

  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());
}

TEST_F(BnplManagerTest,
       OnDidAcceptBnplSuggestion_TriggersExtractionIfTermsNotSeen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableAiBasedAmountExtraction);

  InSequence s;
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi);
  EXPECT_CALL(*mock_amount_extraction_manager_,
              TriggerCheckoutAmountExtractionWithAi());
  EXPECT_CALL(GetBnplUiDelegate(), UpdateBnplIssuerDialogUi);

  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());

  test_api(*bnpl_manager_).OnIssuerSelected(test::GetTestUnlinkedBnplIssuer());

  bnpl_manager_->OnAmountExtractionReturnedFromAi(
      /*extracted_amount_in_micros=*/1'000'000, /*timeout_reached=*/false);
}

TEST_F(BnplManagerTest,
       OnAmountExtractionReturnedFromAi_InvalidAmount_ShowsErrorUi) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());

  InSequence s;
  EXPECT_CALL(GetBnplUiDelegate(), RemoveSelectBnplIssuerOrProgressUi);
  EXPECT_CALL(GetBnplUiDelegate(),
              ShowAutofillErrorUi(
                  AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                      /*is_permanent_error=*/false)));

  bnpl_manager_->OnAmountExtractionReturnedFromAi(
      /*extracted_amount_in_micros=*/std::nullopt,
      /*timeout_reached=*/false);
}

TEST_F(BnplManagerTest, OnAmountExtractionReturnedFromAi_Timeout_ShowsErrorUi) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());

  InSequence s;
  EXPECT_CALL(GetBnplUiDelegate(), RemoveSelectBnplIssuerOrProgressUi());
  EXPECT_CALL(GetBnplUiDelegate(),
              ShowAutofillErrorUi(
                  AutofillErrorDialogContext::WithBnplPermanentOrTemporaryError(
                      /*is_permanent_error=*/false)));

  bnpl_manager_->OnAmountExtractionReturnedFromAi(
      /*extracted_amount_in_micros=*/std::nullopt,
      /*timeout_reached=*/true);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_IOS)
// Tests that `OnIssuerSelected()` calls with a linked issuer where ToS
// acceptance is required will call the payments network interface with the
// request details filled out correctly.
TEST_F(
    BnplManagerTest,
    OnIssuerSelected_CallsGetDetailsForUpdateBnplPaymentInstrument_TosAcceptanceRequired) {
  bnpl_manager_->OnDidAcceptBnplSuggestion(kAmount, base::DoNothing());

  ASSERT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->app_locale,
            kAppLocale);
  ASSERT_EQ(
      test_api(*bnpl_manager_).GetOngoingFlowState()->billing_customer_number,
      kBillingCustomerNumber);

  BnplIssuer issuer = test::GetTestLinkedBnplIssuer(
      IssuerId::kBnplKlarna,
      DenseSet<PaymentInstrument::ActionRequired>{
          PaymentInstrument::ActionRequired::kAcceptTos});

#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(
      *payments_network_interface_,
      GetDetailsForUpdateBnplPaymentInstrument(
          /*request_details=*/
          FieldsAre(
              kAppLocale, kBillingCustomerNumber,
              /*client_behavior_signals=*/
              ElementsAre(
                  ClientBehaviorConstants::kShowAccountEmailInLegalMessage),
              issuer.payment_instrument()->instrument_id(),
              /*type=*/kGetDetailsForAcceptTos,
              /*issuer_id=*/kBnplKlarnaIssuerId),
          /*callback=*/_));
#else   // Desktop only.
  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForUpdateBnplPaymentInstrument(
                  /*request_details=*/
                  FieldsAre(kAppLocale, kBillingCustomerNumber,
                            /*client_behavior_signals=*/IsEmpty(),
                            issuer.payment_instrument()->instrument_id(),
                            /*type=*/kGetDetailsForAcceptTos,
                            /*issuer_id=*/kBnplKlarnaIssuerId),
                  /*callback=*/_));
#endif  // BUILDFLAG(IS_ANDROID)

  OnIssuerSelected(issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->issuer, issuer);
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

#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(
      *payments_network_interface_,
      GetDetailsForCreateBnplPaymentInstrument(
          /*request_details=*/
          FieldsAre(
              kAppLocale, kBillingCustomerNumber,
              /*client_behavior_signals=*/
              ElementsAre(
                  ClientBehaviorConstants::kShowAccountEmailInLegalMessage),
              autofill::ConvertToBnplIssuerIdString(
                  unlinked_issuer.issuer_id())),
          /*callback=*/_));
#else   // Desktop only.
  EXPECT_CALL(*payments_network_interface_,
              GetDetailsForCreateBnplPaymentInstrument(
                  /*request_details=*/
                  FieldsAre(kAppLocale, kBillingCustomerNumber,
                            /*client_behavior_signals=*/IsEmpty(),
                            autofill::ConvertToBnplIssuerIdString(
                                unlinked_issuer.issuer_id())),
                  /*callback=*/_));
#endif  // BUILDFLAG(IS_ANDROID)

  OnIssuerSelected(unlinked_issuer);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->issuer,
            unlinked_issuer);
}
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)

TEST_F(BnplManagerTest,
       OnAmountExtractionReturned_WithTimeout_BeforeBnplSelected) {
  EXPECT_CALL(
      payments_autofill_client(),
      OnPurchaseAmountExtracted(/*bnpl_issuer_contexts=*/IsEmpty(),
                                /*extracted_amount=*/Eq(std::nullopt),
                                /*is_amount_supported_by_any_issuer=*/false,
                                /*app_locale=*/Eq(std::nullopt), _, _));

  bnpl_manager_->OnAmountExtractionReturned(/*extracted_amount=*/std::nullopt,
                                            /*timeout_reached=*/true);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kAmountExtractionTimeout,
      1);
}

TEST_F(BnplManagerTest,
       OnAmountExtractionReturned_WithInvalidAmount_BeforeBnplSelected) {
  EXPECT_CALL(
      payments_autofill_client(),
      OnPurchaseAmountExtracted(/*bnpl_issuer_contexts=*/IsEmpty(),
                                /*extracted_amount=*/Eq(std::nullopt),
                                /*is_amount_supported_by_any_issuer=*/false,
                                /*app_locale=*/Eq(std::nullopt), _, _));

  bnpl_manager_->OnAmountExtractionReturned(/*extracted_amount=*/std::nullopt,
                                            /*timeout_reached=*/false);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kAmountExtractionFailure,
      1);
}

TEST_F(BnplManagerTest,
       OnAmountExtractionReturned_WithUnsupportedAmount_BeforeBnplSelected) {
  const int64_t extracted_amount = 0;
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);
  EXPECT_CALL(payments_autofill_client(),
              OnPurchaseAmountExtracted(
                  /*bnpl_issuer_contexts=*/IsEmpty(), Eq(extracted_amount),
                  /*is_amount_supported_by_any_issuer=*/false,
                  /*app_locale=*/Eq(std::nullopt), _, _));

  bnpl_manager_->OnAmountExtractionReturned(extracted_amount,
                                            /*timeout_reached=*/false);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kCheckoutAmountNotSupported,
      1);
}

TEST_F(BnplManagerTest,
       OnAmountExtractionReturned_WithValidAmount_BeforeBnplSelected) {
  const int64_t extracted_amount = 1000000000;
  SetUpUnlinkedBnplIssuer(/*price_lower_bound_in_micros=*/1'000'000'000,
                          /*price_higher_bound_in_micros=*/2'000'000'000,
                          IssuerId::kBnplZip);
  EXPECT_CALL(payments_autofill_client(),
              OnPurchaseAmountExtracted(
                  /*bnpl_issuer_contexts=*/IsEmpty(), Eq(extracted_amount),
                  /*is_amount_supported_by_any_issuer=*/true,
                  /*app_locale=*/Eq(std::nullopt), _, _));

  bnpl_manager_->OnAmountExtractionReturned(extracted_amount,
                                            /*timeout_reached=*/false);
}

TEST_F(BnplManagerTest,
       OnAmountExtractionReturned_WithTimeout_AfterBnplSelected) {
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/10'000'000,
                        /*price_higher_bound_in_micros=*/200'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/4);
  EXPECT_CALL(
      GetBnplUiDelegate(),
      ShowProgressUi(
          AutofillProgressDialogType::kBnplAmountExtractionProgressUi, _));
  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());
  EXPECT_CALL(
      payments_autofill_client(),
      OnPurchaseAmountExtracted(/*bnpl_issuer_contexts=*/IsEmpty(),
                                /*extracted_amount=*/Eq(std::nullopt),
                                /*is_amount_supported_by_any_issuer=*/false,
                                Optional(Eq(kAppLocale)), _, _));

  bnpl_manager_->OnAmountExtractionReturned(/*extracted_amount=*/std::nullopt,
                                            /*timeout_reached=*/true);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kAmountExtractionTimeout,
      1);
}

TEST_F(BnplManagerTest,
       OnAmountExtractionReturned_WithInvalidAmount_AfterBnplSelected) {
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/10'000'000,
                        /*price_higher_bound_in_micros=*/200'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/4);
  EXPECT_CALL(
      GetBnplUiDelegate(),
      ShowProgressUi(
          AutofillProgressDialogType::kBnplAmountExtractionProgressUi, _));
  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());
  EXPECT_CALL(
      payments_autofill_client(),
      OnPurchaseAmountExtracted(/*bnpl_issuer_contexts=*/IsEmpty(),
                                /*extracted_amount=*/Eq(std::nullopt),
                                /*is_amount_supported_by_any_issuer=*/false,
                                Optional(Eq(kAppLocale)), _, _));

  bnpl_manager_->OnAmountExtractionReturned(/*extracted_amount=*/std::nullopt,
                                            /*timeout_reached=*/false);
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kAmountExtractionFailure,
      1);
}

TEST_F(BnplManagerTest,
       OnAmountExtractionReturned_WithUnsupportedAmount_AfterBnplSelected) {
  const int64_t extracted_amount = 0;
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/10'000'000,
                        /*price_higher_bound_in_micros=*/3'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1);
  EXPECT_CALL(
      GetBnplUiDelegate(),
      ShowProgressUi(
          AutofillProgressDialogType::kBnplAmountExtractionProgressUi, _));
  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer(IssuerId::kBnplAffirm, _))
      .WillByDefault(Return(true));
  std::vector<BnplIssuerContext> issuer_contexts;
  EXPECT_CALL(
      payments_autofill_client(),
      OnPurchaseAmountExtracted(_, Eq(extracted_amount),
                                /*is_amount_supported_by_any_issuer=*/false,
                                Optional(Eq(kAppLocale)), _, _))
      .WillOnce([&](base::span<const payments::BnplIssuerContext> contexts,
                    auto, auto, auto, auto, auto) {
        issuer_contexts.assign(contexts.begin(), contexts.end());
        return true;
      });

  bnpl_manager_->OnAmountExtractionReturned(extracted_amount,
                                            /*timeout_reached=*/false);

  EXPECT_THAT(
      issuer_contexts,
      ElementsAre(EqualsBnplIssuerContext(
          IssuerId::kBnplAffirm,
          BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow)));
  histogram_tester_->ExpectUniqueSample(
      "Autofill.Bnpl.SuggestionUnavailableReason",
      autofill_metrics::BnplSuggestionUnavailableReason::
          kCheckoutAmountNotSupported,
      1);
}

TEST_F(BnplManagerTest,
       OnAmountExtractionReturned_WithValidAmount_AfterBnplSelected) {
  const int64_t extracted_amount = 1000000000;
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/10'000'000,
                        /*price_higher_bound_in_micros=*/3'000'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/1);
  EXPECT_CALL(
      GetBnplUiDelegate(),
      ShowProgressUi(
          AutofillProgressDialogType::kBnplAmountExtractionProgressUi, _));
  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer(IssuerId::kBnplAffirm, _))
      .WillByDefault(Return(true));
  std::vector<BnplIssuerContext> issuer_contexts;
  EXPECT_CALL(
      payments_autofill_client(),
      OnPurchaseAmountExtracted(_, Eq(extracted_amount),
                                /*is_amount_supported_by_any_issuer=*/true,
                                Optional(Eq(kAppLocale)), _, _))
      .WillOnce([&](base::span<const payments::BnplIssuerContext> contexts,
                    auto, auto, auto, auto, auto) {
        issuer_contexts.assign(contexts.begin(), contexts.end());
        return true;
      });

  bnpl_manager_->OnAmountExtractionReturned(extracted_amount,
                                            /*timeout_reached=*/false);

  EXPECT_THAT(issuer_contexts, ElementsAre(EqualsBnplIssuerContext(
                                   IssuerId::kBnplAffirm,
                                   BnplIssuerEligibilityForPage::kIsEligible)));
}

TEST_F(BnplManagerTest,
       OnDidAcceptBnplSuggestion_WhenValidAmount_ForwardsCallToDelegate) {
  SetUpLinkedBnplIssuer(/*price_lower_bound_in_micros=*/10'000'000,
                        /*price_higher_bound_in_micros=*/200'000'000,
                        IssuerId::kBnplAffirm,
                        /*instrument_id=*/4);
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer(IssuerId::kBnplAffirm, _))
      .WillByDefault(Return(true));
  std::vector<BnplIssuerContext> issuer_context;
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi)
      .WillOnce(MoveArg<0>(&issuer_context));

  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/10'000'000,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());

  EXPECT_THAT(issuer_context, ElementsAre(EqualsBnplIssuerContext(
                                  IssuerId::kBnplAffirm,
                                  BnplIssuerEligibilityForPage::kIsEligible)));
}

TEST_F(
    BnplManagerTest,
    OnDidAcceptBnplSuggestion_WhenAmountIsNotSet_ShowProgressUiNotSelectionUi) {
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi).Times(0);
  EXPECT_CALL(
      GetBnplUiDelegate(),
      ShowProgressUi(
          AutofillProgressDialogType::kBnplAmountExtractionProgressUi, _))
      .WillOnce(base::test::RunOnceCallback<1>());

  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/std::nullopt,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());
}

TEST_F(BnplManagerTest, OnTouchToFillIssuerSelectionCancelled_ResetsFlow) {
  EXPECT_CALL(GetBnplUiDelegate(), ShowSelectBnplIssuerUi)
      .WillOnce(base::test::RunOnceCallback<3>());

  bnpl_manager_->OnDidAcceptBnplSuggestion(
      /*final_checkout_amount=*/10'000'000,
      /*on_bnpl_vcn_fetched_callback=*/base::DoNothing());

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

TEST_F(BnplManagerTest, OnPurchaseAmountExtracted_IssuerSelectedCallback) {
  const BnplIssuer linked_issuer = test::GetTestLinkedBnplIssuer();
  // This initializes `ongoing_flow_state_`.
  test_api(*bnpl_manager_)
      .PopulateManagerWithUserAndBnplIssuerDetails(
          kBillingCustomerNumber,
          base::NumberToString(
              linked_issuer.payment_instrument()->instrument_id()),
          kRiskData, kContextToken, kRedirectUrl, linked_issuer);
  EXPECT_CALL(payments_autofill_client(), OnPurchaseAmountExtracted)
      .WillOnce([&](auto, auto, auto, auto,
                    base::OnceCallback<void(autofill::BnplIssuer)>
                        selected_issuer_callback,
                    auto) {
        std::move(selected_issuer_callback).Run(linked_issuer);
        return true;
      });

  bnpl_manager_->OnAmountExtractionReturned(/*extracted_amount=*/10'000'000,
                                            /*timeout_reached=*/false);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState()->issuer,
            linked_issuer);
}

TEST_F(BnplManagerTest, OnPurchaseAmountExtracted_CancelCallback) {
  const BnplIssuer linked_issuer = test::GetTestLinkedBnplIssuer();
  // This initializes `ongoing_flow_state_`.
  test_api(*bnpl_manager_)
      .PopulateManagerWithUserAndBnplIssuerDetails(
          kBillingCustomerNumber,
          base::NumberToString(
              linked_issuer.payment_instrument()->instrument_id()),
          kRiskData, kContextToken, kRedirectUrl, linked_issuer);
  EXPECT_CALL(payments_autofill_client(), OnPurchaseAmountExtracted)
      .WillOnce(
          [&](auto, auto, auto, auto, auto, base::OnceClosure cancel_callback) {
            std::move(cancel_callback).Run();
            return true;
          });

  bnpl_manager_->OnAmountExtractionReturned(/*extracted_amount=*/10'000'000,
                                            /*timeout_reached=*/false);

  EXPECT_EQ(test_api(*bnpl_manager_).GetOngoingFlowState(), nullptr);
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace autofill::payments
