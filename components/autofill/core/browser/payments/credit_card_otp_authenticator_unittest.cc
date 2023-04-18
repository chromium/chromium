// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/test_authentication_requester.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {
const char kTestChallengeId[] = "arbitrary challenge id";
const char kTestNumber[] = "4234567890123456";
const char16_t kTestNumber16[] = u"4234567890123456";
const char16_t kMaskedPhoneNumber[] = u"(***)-***-5678";
const char16_t kMaskedEmailAddress[] = u"a******b@google.com";
const int64_t kTestBillingCustomerNumber = 123456;
}  // namespace

class CreditCardOtpAuthenticatorTestBase : public testing::Test {
 public:
  CreditCardOtpAuthenticatorTestBase() = default;
  ~CreditCardOtpAuthenticatorTestBase() override = default;

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_manager_.Init(/*profile_database=*/nullptr,
                                /*account_database=*/nullptr,
                                /*pref_service=*/autofill_client_.GetPrefs(),
                                /*local_state=*/autofill_client_.GetPrefs(),
                                /*identity_manager=*/nullptr,
                                /*history_service=*/nullptr,
                                /*sync_service=*/nullptr,
                                /*strike_database=*/nullptr,
                                /*image_fetcher=*/nullptr,
                                /*is_off_the_record=*/false);
    personal_data_manager_.SetPrefService(autofill_client_.GetPrefs());

    requester_ = std::make_unique<TestAuthenticationRequester>();

    payments_client_ = new payments::TestPaymentsClient(
        autofill_client_.GetURLLoaderFactory(),
        autofill_client_.GetIdentityManager(), &personal_data_manager_);
    autofill_client_.set_test_payments_client(
        std::unique_ptr<payments::TestPaymentsClient>(payments_client_));
    authenticator_ =
        std::make_unique<CreditCardOtpAuthenticator>(&autofill_client_);

    card_ = test::GetMaskedServerCard();
    card_.set_record_type(CreditCard::VIRTUAL_CARD);
  }

  void TearDown() override {
    // Order of destruction is important as AutofillDriver relies on
    // PersonalDataManager to be around when it gets destroyed.
    personal_data_manager_.SetPrefService(nullptr);
  }

  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan,
                       bool server_returned_decline_details = false) {
    payments::PaymentsClient::UnmaskResponseDetails response;
    if (result != AutofillClient::PaymentsRpcResult::kSuccess) {
      if (server_returned_decline_details) {
        AutofillErrorDialogContext context;
        context.type = AutofillErrorDialogType::kVirtualCardTemporaryError;
        context.server_returned_title = "test_server_returned_title";
        context.server_returned_description =
            "test_server_returned_description";
        response.autofill_error_dialog_context = std::move(context);
      }
      authenticator_->OnDidGetRealPan(result, response);
      return;
    }
    response.real_pan = real_pan;
    response.dcvv = "123";
    response.expiration_month = test::NextMonth();
    response.expiration_year = test::NextYear();
    response.card_type = AutofillClient::PaymentsRpcCardType::kVirtualCard;
    authenticator_->OnDidGetRealPan(result, response);
  }

  void OnDidGetRealPanWithFlowStatus(const std::string& flow_status,
                                     const std::string& context_token) {
    payments::PaymentsClient::UnmaskResponseDetails response;
    response.flow_status = flow_status;
    response.context_token = context_token;
    response.card_type = AutofillClient::PaymentsRpcCardType::kVirtualCard;
    authenticator_->OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                                    response);
  }

  std::string OtpAuthenticatorContextToken() {
    return authenticator_->ContextTokenForTesting();
  }

  void verifySelectChallengeOptionRequest(const std::string& context_token,
                                          int64_t billing_customer_number) {
    const payments::PaymentsClient::SelectChallengeOptionRequestDetails*
        request = payments_client_->select_challenge_option_request();
    EXPECT_EQ(request->context_token, context_token);
    EXPECT_EQ(request->billing_customer_number, billing_customer_number);
    // Selected challenge option should stay the same for the entire session.
    EXPECT_EQ(request->selected_challenge_option.id,
              selected_otp_challenge_option_.id);
  }

  void CreateSelectedOtpChallengeOption(CardUnmaskChallengeOptionType type) {
    selected_otp_challenge_option_.type = type;
    selected_otp_challenge_option_.id =
        CardUnmaskChallengeOption::ChallengeOptionId(kTestChallengeId);
    if (type == CardUnmaskChallengeOptionType::kSmsOtp) {
      selected_otp_challenge_option_.challenge_info = kMaskedPhoneNumber;
    } else if (type == CardUnmaskChallengeOptionType::kEmailOtp) {
      selected_otp_challenge_option_.challenge_info = kMaskedEmailAddress;
    }
  }

 protected:
  std::unique_ptr<TestAuthenticationRequester> requester_;
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  TestPersonalDataManager personal_data_manager_;
  raw_ptr<payments::TestPaymentsClient> payments_client_;
  std::unique_ptr<CreditCardOtpAuthenticator> authenticator_;
  CreditCard card_;
  CardUnmaskChallengeOption selected_otp_challenge_option_;
};

class CreditCardOtpAuthenticatorTest
    : public CreditCardOtpAuthenticatorTestBase,
      public testing::WithParamInterface<CardUnmaskChallengeOptionType> {
 public:
  CreditCardOtpAuthenticatorTest() = default;
  ~CreditCardOtpAuthenticatorTest() override = default;

  void SetUp() override {
    CreditCardOtpAuthenticatorTestBase::SetUp();
    CreateSelectedOtpChallengeOption(GetParam());
  }

  std::string GetOtpAuthType() {
    return autofill_metrics::GetOtpAuthType(GetParam());
  }
};

TEST_P(CreditCardOtpAuthenticatorTest, AuthenticateServerCardSuccess) {
  base::HistogramTester histogram_tester;
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsClient will ack the select challenge
  // option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the SelectChallengeRequest content.
  verifySelectChallengeOptionRequest(
      /*context_token=*/"context_token_from_previous_unmask_response",
      kTestBillingCustomerNumber);
  // Verify the context token is updated with SelectChallengeOption response.
  EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
  EXPECT_NE(OtpAuthenticatorContextToken(),
            "context_token_from_previous_unmask_response");

  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsClient just stores the unmask request detail, won't invoke the
  // callback. OnDidGetRealPan below will manually invoke the callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");
  // Verify that the otp is correctly set in UnmaskRequestDetails.
  EXPECT_EQ(payments_client_->unmask_request()->otp, u"111111");
  // Also verify that risk data is set in UnmaskRequestDetails.
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());

  // Simulate server returns success and invoke the callback.
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, kTestNumber);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_TRUE(*(requester_->did_succeed()));
  EXPECT_EQ(kTestNumber16, requester_->number());

  // Ensures the metrics have been logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Result",
      autofill_metrics::OtpAuthEvent::kSuccess, 1);
  histogram_tester.ExpectTotalCount("Autofill.OtpAuth." + GetOtpAuthType() +
                                        ".RequestLatency.UnmaskCardRequest",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Autofill.OtpAuth." + GetOtpAuthType() +
          ".RequestLatency.SelectChallengeOptionRequest",
      1);
}

TEST_P(CreditCardOtpAuthenticatorTest, SelectChallengeOptionFailsWithVcnError) {
  base::HistogramTester histogram_tester;
  // Simulate server returns virtual card permanent failure.
  payments_client_->set_select_challenge_option_result(
      AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure);

  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsClient will ack the select challenge
  // option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the SelectChallengeRequest content.
  verifySelectChallengeOptionRequest(
      /*context_token=*/"context_token_from_previous_unmask_response",
      kTestBillingCustomerNumber);
  // Verify error dialog is shown.
  EXPECT_TRUE(autofill_client_.virtual_card_error_dialog_shown());
  // Ensure the OTP authenticator is reset.
  EXPECT_TRUE(OtpAuthenticatorContextToken().empty());
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_FALSE(*(requester_->did_succeed()));

  // Ensures the metrics have been logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Result",
      autofill_metrics::OtpAuthEvent::
          kSelectedChallengeOptionVirtualCardRetrievalError,
      1);
  histogram_tester.ExpectTotalCount(
      "Autofill.OtpAuth." + GetOtpAuthType() +
          ".RequestLatency.SelectChallengeOptionRequest",
      1);
}

TEST_P(CreditCardOtpAuthenticatorTest,
       SelectChallengeOptionFailsWithOtherErrors) {
  base::HistogramTester histogram_tester;
  // Simulate server returns non-virtual card permanent failure, e.g. response
  // not complete.
  payments_client_->set_select_challenge_option_result(
      AutofillClient::PaymentsRpcResult::kPermanentFailure);

  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsClient will ack the select challenge
  // option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the SelectChallengeRequest content.
  verifySelectChallengeOptionRequest(
      /*context_token=*/"context_token_from_previous_unmask_response",
      kTestBillingCustomerNumber);
  // Verify error dialog is shown.
  EXPECT_TRUE(autofill_client_.virtual_card_error_dialog_shown());
  // Ensure the OTP authenticator is reset.
  EXPECT_TRUE(OtpAuthenticatorContextToken().empty());
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_FALSE(*(requester_->did_succeed()));

  // Ensures the metrics have been logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Result",
      autofill_metrics::OtpAuthEvent::kSelectedChallengeOptionGenericError, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.OtpAuth." + GetOtpAuthType() +
          ".RequestLatency.SelectChallengeOptionRequest",
      1);
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthServerVcnError) {
  for (bool server_returned_decline_details : {true, false}) {
    base::HistogramTester histogram_tester;
    // Simulate user selects OTP challenge option. Current context_token is from
    // previous unmask response. TestPaymentsClient will ack the select
    // challenge option request and directly invoke the callback.
    authenticator_->OnChallengeOptionSelected(
        &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
        /*context_token=*/"context_token_from_previous_unmask_response",
        /*billing_customer_number=*/kTestBillingCustomerNumber);
    // Verify the context token is updated with SelectChallengeOption response.
    EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
    EXPECT_NE(OtpAuthenticatorContextToken(),
              "context_token_from_previous_unmask_response");
    // TODO(crbug.com/1243475): Verify the otp dialog is shown.

    // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
    // TestPaymentsClient just stores the unmask request detail, won't invoke
    // the callback. OnDidGetRealPan below will manually invoke the callback.
    authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");
    // Simulate server returns virtual card retrieval try again failure. We will
    // show the error dialog and end session.
    OnDidGetRealPan(
        AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
        /*real_pan=*/"", server_returned_decline_details);
    // Verify error dialog is shown.
    EXPECT_TRUE(autofill_client_.virtual_card_error_dialog_shown());
    if (server_returned_decline_details) {
      AutofillErrorDialogContext context =
          autofill_client_.autofill_error_dialog_context();
      EXPECT_EQ(context.type,
                AutofillErrorDialogType::kVirtualCardTemporaryError);
      EXPECT_EQ(*context.server_returned_title, "test_server_returned_title");
      EXPECT_EQ(*context.server_returned_description,
                "test_server_returned_description");
    }
    // Ensure the OTP authenticator is reset.
    EXPECT_TRUE(OtpAuthenticatorContextToken().empty());
    ASSERT_TRUE(requester_->did_succeed().has_value());
    EXPECT_FALSE(*(requester_->did_succeed()));

    // Ensures the metrics have been logged correctly.
    histogram_tester.ExpectUniqueSample(
        "Autofill.OtpAuth." + GetOtpAuthType() + ".Attempt", true, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.OtpAuth." + GetOtpAuthType() + ".Result",
        autofill_metrics::OtpAuthEvent::kUnmaskCardVirtualCardRetrievalError,
        1);
    histogram_tester.ExpectTotalCount("Autofill.OtpAuth." + GetOtpAuthType() +
                                          ".RequestLatency.UnmaskCardRequest",
                                      1);
    histogram_tester.ExpectTotalCount(
        "Autofill.OtpAuth." + GetOtpAuthType() +
            ".RequestLatency.SelectChallengeOptionRequest",
        1);
  }
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthServerNonVcnError) {
  base::HistogramTester histogram_tester;
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsClient will ack the select challenge
  // option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the context token is updated with SelectChallengeOption response.
  EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
  EXPECT_NE(OtpAuthenticatorContextToken(),
            "context_token_from_previous_unmask_response");
  // TODO(crbug.com/1243475): Verify the otp dialog is shown.

  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsClient just stores the unmask request detail, won't invoke the
  // callback. OnDidGetRealPan below will manually invoke the callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");
  // Simulate server returns non-Vcn try again failure. We will reuse virtual
  // card error dialog and end session.
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kTryAgainFailure,
                  /*real_pan=*/"");
  // Verify error dialog is shown.
  EXPECT_TRUE(autofill_client_.virtual_card_error_dialog_shown());
  // Ensure the OTP authenticator is reset.
  EXPECT_TRUE(OtpAuthenticatorContextToken().empty());
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_FALSE(*(requester_->did_succeed()));

  // Ensures the metrics have been logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Result",
      autofill_metrics::OtpAuthEvent::kUnmaskCardAuthError, 1);
  histogram_tester.ExpectTotalCount("Autofill.OtpAuth." + GetOtpAuthType() +
                                        ".RequestLatency.UnmaskCardRequest",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Autofill.OtpAuth." + GetOtpAuthType() +
          ".RequestLatency.SelectChallengeOptionRequest",
      1);
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthMismatchThenRetry) {
  base::HistogramTester histogram_tester;
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsClient will ack the select challenge
  // option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the context token is updated with SelectChallengeOption response.
  EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
  EXPECT_NE(OtpAuthenticatorContextToken(),
            "context_token_from_previous_unmask_response");
  // TODO(crbug.com/1243475): Verify the otp dialog is shown.

  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsClient just stores the unmask request detail, won't invoke the
  // callback. OnDidGetRealPan below will manually invoke the callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"222222");
  // Verify that the otp is correctly set in UnmaskRequestDetails.
  EXPECT_EQ(payments_client_->unmask_request()->otp, u"222222");
  // Also verify that risk data is set in UnmaskRequestDetails.
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());

  // Simulate otp mismatch, server returns flow_status indicating incorrect otp.
  OnDidGetRealPanWithFlowStatus(
      /*flow_status=*/"FLOW_STATUS_INCORRECT_OTP",
      /*context_token=*/"context_token_from_incorrect_otp");
  // Verify the context token is updated with unmask response.
  EXPECT_EQ(OtpAuthenticatorContextToken(), "context_token_from_incorrect_otp");
  // TODO(crbug.com/1243475): Verify the otp dialog is updated with the mismatch
  // info.

  // Simulate user types in another otp and click 'Confirm' again.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"333333");
  // Verify that the new otp is correctly set in UnmaskRequestDetails together
  // with the latest context token.
  EXPECT_EQ(payments_client_->unmask_request()->otp, u"333333");
  EXPECT_EQ(payments_client_->unmask_request()->context_token,
            "context_token_from_incorrect_otp");
  // Also verify that risk data is still set in UnmaskRequestDetails.
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());

  // Simulate server returns success for the second try and invoke the callback.
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, kTestNumber);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_TRUE(*(requester_->did_succeed()));
  EXPECT_EQ(kTestNumber16, requester_->number());

  // Ensures the metrics have been logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Result",
      autofill_metrics::OtpAuthEvent::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".RetriableError",
      autofill_metrics::OtpAuthEvent::kOtpMismatch, 1);
  histogram_tester.ExpectTotalCount("Autofill.OtpAuth." + GetOtpAuthType() +
                                        ".RequestLatency.UnmaskCardRequest",
                                    2);
  histogram_tester.ExpectTotalCount(
      "Autofill.OtpAuth." + GetOtpAuthType() +
          ".RequestLatency.SelectChallengeOptionRequest",
      1);
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthExpiredThenResendOtp) {
  base::HistogramTester histogram_tester;
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsClient will ack the select challenge
  // option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the SelectChallengeRequest content.
  verifySelectChallengeOptionRequest(
      /*context_token=*/"context_token_from_previous_unmask_response",
      kTestBillingCustomerNumber);
  // Verify the context token is updated with SelectChallengeOption response.
  EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
  EXPECT_NE(OtpAuthenticatorContextToken(),
            "context_token_from_previous_unmask_response");
  // TODO(crbug.com/1243475): Verify the otp dialog is shown.

  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsClient just stores the unmask request detail, won't invoke the
  // callback. OnDidGetRealPan below will manually invoke the callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"4444444");
  // Verify that the otp is correctly set in UnmaskRequestDetails.
  EXPECT_EQ(payments_client_->unmask_request()->otp, u"4444444");
  // Also verify that risk data is set in UnmaskRequestDetails.
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());

  // Simulate otp expired, server returns flow_status indicating expired otp.
  OnDidGetRealPanWithFlowStatus(
      /*flow_status=*/"FLOW_STATUS_EXPIRED_OTP",
      /*context_token=*/"context_token_from_expired_otp");
  // Verify the context token is updated with unmask response.
  EXPECT_EQ(OtpAuthenticatorContextToken(), "context_token_from_expired_otp");
  // TODO(crbug.com/1243475): Verify the otp dialog is updated with the otp
  // expired info.

  // Simulate user clicks "Get new code" from the UI, which calls
  // SendSelectChallengeOptionRequest() again. This will send the same selected
  // challenge option with the new context token.
  authenticator_->SendSelectChallengeOptionRequest();
  // Verify the second SelectChallengeRequest is correctly set, the only
  // difference from the previous call is the context_token.
  verifySelectChallengeOptionRequest(
      /*context_token=*/"context_token_from_expired_otp",
      kTestBillingCustomerNumber);

  // Simulate user receives the new otp and types in the new otp.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"555555");
  // Verify that the new otp is correctly set in UnmaskRequestDetails together
  // with the latest context token.
  EXPECT_EQ(payments_client_->unmask_request()->otp, u"555555");
  // Note here is NOT EQUAL. The context token is from the new select challenge
  // option response.
  EXPECT_NE(payments_client_->unmask_request()->context_token,
            "context_token_from_expired_otp");
  // Also verify that risk data is still set in UnmaskRequestDetails.
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());

  // Simulate server returns success for the second try and invoke the callback.
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess, kTestNumber);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_TRUE(*(requester_->did_succeed()));
  EXPECT_EQ(kTestNumber16, requester_->number());

  // Ensures the metrics have been logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Result",
      autofill_metrics::OtpAuthEvent::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".RetriableError",
      autofill_metrics::OtpAuthEvent::kOtpExpired, 1);
  histogram_tester.ExpectTotalCount("Autofill.OtpAuth." + GetOtpAuthType() +
                                        ".RequestLatency.UnmaskCardRequest",
                                    2);
  histogram_tester.ExpectTotalCount(
      "Autofill.OtpAuth." + GetOtpAuthType() +
          ".RequestLatency.SelectChallengeOptionRequest",
      2);
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthCancelled) {
  base::HistogramTester histogram_tester;
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsClient will ack the select challenge
  // option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the SelectChallengeRequest content.
  verifySelectChallengeOptionRequest(
      /*context_token=*/"context_token_from_previous_unmask_response",
      kTestBillingCustomerNumber);
  // Verify the context token is updated with SelectChallengeOption response.
  EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
  EXPECT_NE(OtpAuthenticatorContextToken(),
            "context_token_from_previous_unmask_response");

  // Simulate user closes the otp input dialog.
  authenticator_->OnUnmaskPromptClosed(/*user_closed_dialog=*/true);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_FALSE(*(requester_->did_succeed()));

  // Ensures the metrics have been logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OtpAuth." + GetOtpAuthType() + ".Result",
      autofill_metrics::OtpAuthEvent::kFlowCancelled, 1);
  histogram_tester.ExpectTotalCount("Autofill.OtpAuth." + GetOtpAuthType() +
                                        ".RequestLatency.UnmaskCardRequest",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Autofill.OtpAuth." + GetOtpAuthType() +
          ".RequestLatency.SelectChallengeOptionRequest",
      1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CreditCardOtpAuthenticatorTest,
    testing::Values(CardUnmaskChallengeOptionType::kSmsOtp,
                    CardUnmaskChallengeOptionType::kEmailOtp));

// Params of the CreditCardOtpAuthenticatorCardMetadataTest:
// -- bool card_name_available;
// -- bool card_art_available;
// -- bool metadata_enabled;
class CreditCardOtpAuthenticatorCardMetadataTest
    : public CreditCardOtpAuthenticatorTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  CreditCardOtpAuthenticatorCardMetadataTest() = default;
  ~CreditCardOtpAuthenticatorCardMetadataTest() override = default;

  void SetUp() override {
    CreditCardOtpAuthenticatorTestBase::SetUp();
    CreateSelectedOtpChallengeOption(CardUnmaskChallengeOptionType::kSmsOtp);
  }

  bool CardNameAvailable() { return std::get<0>(GetParam()); }
  bool CardArtAvailable() { return std::get<1>(GetParam()); }
  bool MetadataEnabled() { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(,
                         CreditCardOtpAuthenticatorCardMetadataTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

TEST_P(CreditCardOtpAuthenticatorCardMetadataTest, MetadataSignal) {
  base::test::ScopedFeatureList metadata_feature_list;
  if (MetadataEnabled()) {
    metadata_feature_list.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillEnableCardProductName,
                              features::kAutofillEnableCardArtImage},
        /*disabled_features=*/{});
  } else {
    metadata_feature_list.InitWithFeaturesAndParameters(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kAutofillEnableCardProductName,
                               features::kAutofillEnableCardArtImage});
  }
  if (CardNameAvailable()) {
    card_.set_product_description(u"fake product description");
  }
  if (CardArtAvailable()) {
    card_.set_card_art_url(GURL("https://www.example.com"));
  }

  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsClient will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);

  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsClient just stores the unmask request detail, won't invoke
  // the callback. OnDidGetRealPan below will manually invoke the callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");
  // Verify that the otp is correctly set in UnmaskRequestDetails.
  EXPECT_EQ(payments_client_->unmask_request()->otp, u"111111");
  // Also verify that risk data is set in UnmaskRequestDetails.
  EXPECT_FALSE(payments_client_->unmask_request()->risk_data.empty());
  std::vector<ClientBehaviorConstants> signals =
      payments_client_->unmask_request()->client_behavior_signals;
  if (MetadataEnabled() && CardNameAvailable() && CardArtAvailable()) {
    EXPECT_NE(
        signals.end(),
        base::ranges::find(
            signals,
            ClientBehaviorConstants::kShowingCardArtImageAndCardProductName));
  } else {
    EXPECT_TRUE(signals.empty());
  }
}

}  // namespace autofill
