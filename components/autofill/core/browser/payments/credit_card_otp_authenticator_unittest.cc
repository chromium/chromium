// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_authentication_requester.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;

const char kTestChallengeId[] = "arbitrary challenge id";
const char kTestNumber[] = "4234567890123456";
const char16_t kTestNumber16[] = u"4234567890123456";
const char16_t kMaskedPhoneNumber[] = u"(***)-***-5678";
const char16_t kMaskedEmailAddress[] = u"a******b@google.com";
const int64_t kTestBillingCustomerNumber = 123456;

class CreditCardOtpAuthenticatorTestBase : public testing::Test {
 public:
  CreditCardOtpAuthenticatorTestBase() = default;
  ~CreditCardOtpAuthenticatorTestBase() override = default;

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data().SetPrefService(autofill_client().GetPrefs());
    personal_data().SetSyncServiceForTest(&sync_service_);
    personal_data()
        .test_payments_data_manager()
        .SetAutofillPaymentMethodsEnabled(true);

    requester_ = std::make_unique<TestAuthenticationRequester>();
    payments_autofill_client().set_payments_network_interface(
        std::make_unique<payments::TestPaymentsNetworkInterface>(
            autofill_client_.GetURLLoaderFactory(),
            autofill_client_.GetIdentityManager(), &personal_data()));
    authenticator_ =
        std::make_unique<CreditCardOtpAuthenticator>(&autofill_client_);

    card_ = test::GetMaskedServerCard();
  }

  void TearDown() override {
    // Order of destruction is important as AutofillDriver relies on
    // PersonalDataManager to be around when it gets destroyed.
    personal_data().SetPrefService(nullptr);
  }

  void OnDidGetRealPan(PaymentsRpcResult result,
                       const std::string& real_pan,
                       bool server_returned_decline_details = false) {
    payments::UnmaskResponseDetails response;
    if (result != PaymentsRpcResult::kSuccess) {
      if (server_returned_decline_details) {
        AutofillErrorDialogContext context;
        if (result == payments::PaymentsAutofillClient::PaymentsRpcResult::
                          kVcnRetrievalPermanentFailure ||
            result == payments::PaymentsAutofillClient::PaymentsRpcResult::
                          kVcnRetrievalTryAgainFailure) {
          context.type = AutofillErrorDialogType::kVirtualCardTemporaryError;
        } else {
          context.type =
              AutofillErrorDialogType::kCardInfoRetrievalTemporaryError;
        }
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
    response.card_type =
        card_.record_type() == CreditCard::RecordType::kVirtualCard
            ? payments::PaymentsAutofillClient::PaymentsRpcCardType::
                  kVirtualCard
            : payments::PaymentsAutofillClient::PaymentsRpcCardType::
                  kServerCard;
    authenticator_->OnDidGetRealPan(result, response);
  }

  void OnDidGetRealPanWithFlowStatus(const std::string& flow_status,
                                     const std::string& context_token) {
    payments::UnmaskResponseDetails response;
    response.flow_status = flow_status;
    response.context_token = context_token;
    response.card_type =
        card_.record_type() == CreditCard::RecordType::kVirtualCard
            ? payments::PaymentsAutofillClient::PaymentsRpcCardType::
                  kVirtualCard
            : payments::PaymentsAutofillClient::PaymentsRpcCardType::
                  kServerCard;
    authenticator_->OnDidGetRealPan(PaymentsRpcResult::kSuccess, response);
  }

  std::string OtpAuthenticatorContextToken() {
    return authenticator_->ContextTokenForTesting();
  }

  void VerifySelectChallengeOptionRequest(const std::string& context_token,
                                          int64_t billing_customer_number) {
    const payments::SelectChallengeOptionRequestDetails* request =
        payments_network_interface().select_challenge_option_request();
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
  TestPersonalDataManager& personal_data() {
    return autofill_client().GetPersonalDataManager();
  }

  TestAutofillClient& autofill_client() { return autofill_client_; }
  payments::TestPaymentsAutofillClient& payments_autofill_client() {
    return *autofill_client().GetPaymentsAutofillClient();
  }
  payments::TestPaymentsNetworkInterface& payments_network_interface() {
    return static_cast<payments::TestPaymentsNetworkInterface&>(
        *payments_autofill_client().GetPaymentsNetworkInterface());
  }

  std::unique_ptr<TestAuthenticationRequester> requester_;
  base::test::TaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<CreditCardOtpAuthenticator> authenticator_;
  CreditCard card_;
  CardUnmaskChallengeOption selected_otp_challenge_option_;
};

class CreditCardOtpAuthenticatorTest
    : public CreditCardOtpAuthenticatorTestBase,
      public ::testing::WithParamInterface<
          std::tuple<CardUnmaskChallengeOptionType, CreditCard::RecordType>> {
 public:
  CreditCardOtpAuthenticatorTest() = default;
  ~CreditCardOtpAuthenticatorTest() override = default;

  void SetUp() override {
    CreditCardOtpAuthenticatorTestBase::SetUp();
    CardUnmaskChallengeOptionType option_type = std::get<0>(GetParam());
    record_type_ = std::get<1>(GetParam());
    CreateSelectedOtpChallengeOption(option_type);

    // Masked Server card is only tested for SmsOtp.
    if (option_type == CardUnmaskChallengeOptionType::kSmsOtp &&
        record_type_ == CreditCard::RecordType::kMaskedServerCard) {
      card_.set_card_info_retrieval_enrollment_state(
          CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled);
    }
    card_.set_record_type(record_type_);
  }

  std::string GetOtpAuthType() {
    switch (std::get<0>(GetParam())) {
      case CardUnmaskChallengeOptionType::kSmsOtp:
        return "SmsOtp";
      case CardUnmaskChallengeOptionType::kEmailOtp:
        return "EmailOtp";
      default:
        NOTREACHED();
    }
  }

  std::string GetCardType() {
    switch (record_type_) {
      case CreditCard::RecordType::kVirtualCard:
        return "VirtualCard";
      case CreditCard::RecordType::kFullServerCard:
      case CreditCard::RecordType::kMaskedServerCard:
        return "ServerCard";
      case CreditCard::RecordType::kLocalCard:
        return "LocalCard";
      default:
        NOTREACHED();
    }
  }

  // Recordtype of unmasked server card changes to kFullServerCard;
  CreditCard::RecordType GetUnmaskedCardRecordType() {
    return record_type_ == CreditCard::RecordType::kMaskedServerCard
               ? CreditCard::RecordType::kFullServerCard
               : record_type_;
  }

 private:
  CreditCard::RecordType record_type_;
};

// Test the yellow path SMAS based OTP challenege flow.
TEST_P(CreditCardOtpAuthenticatorTest, AuthenticateServerCardSuccess) {
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will directly invoke
  // m the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the SelectChallengeRequest content.
  VerifySelectChallengeOptionRequest(
      /*context_token=*/"context_token_from_previous_unmask_response",
      kTestBillingCustomerNumber);
  // Verify the context token is updated with SelectChallengeOption response.
  EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
  EXPECT_NE(OtpAuthenticatorContextToken(),
            "context_token_from_previous_unmask_response");

  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsNetworkInterface just stores the unmask request detail, won't
  // invoke the callback. OnDidGetRealPan below will manually invoke the
  // callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");
  // Verify the selected challenge option is correctly set in
  // UnmaskRequestDetails.
  EXPECT_EQ(payments_network_interface()
                .unmask_request()
                ->selected_challenge_option->id,
            selected_otp_challenge_option_.id);
  // Verify that the otp is correctly set in UnmaskRequestDetails.
  EXPECT_EQ(payments_network_interface().unmask_request()->otp, u"111111");
  // Also verify that risk data is set in UnmaskRequestDetails.
  EXPECT_FALSE(
      payments_network_interface().unmask_request()->risk_data.empty());

  // Simulate server returns success and invoke the callback.
  OnDidGetRealPan(PaymentsRpcResult::kSuccess, kTestNumber);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_TRUE(*(requester_->did_succeed()));
  EXPECT_EQ(kTestNumber16, requester_->number());
  EXPECT_EQ(GetUnmaskedCardRecordType(), requester_->record_type());
}

TEST_P(CreditCardOtpAuthenticatorTest, AuthenticateServerCardSuccessMetrics) {
  base::HistogramTester histogram_tester;
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will directly invoke
  // m the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);

  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsNetworkInterface just stores the unmask request detail, won't
  // invoke the callback. OnDidGetRealPan below will manually invoke the
  // callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");

  // Simulate server returns success and invoke the callback.
  OnDidGetRealPan(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                  kTestNumber);

  // Ensures the metrics have been logged correctly.
  std::string base_histogram_name =
      "Autofill.OtpAuth." + GetCardType() + "." + GetOtpAuthType();
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".Attempt", true,
                                      1);
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".Result",
                                      autofill_metrics::OtpAuthEvent::kSuccess,
                                      1);
  histogram_tester.ExpectTotalCount(
      base_histogram_name + ".RequestLatency.UnmaskCardRequest", 1);
  histogram_tester.ExpectTotalCount(
      base_histogram_name + ".RequestLatency.SelectChallengeOptionRequest", 1);
}

TEST_P(CreditCardOtpAuthenticatorTest, SelectChallengeOptionFailsWithVcnError) {
  // Simulate server returns virtual card permanent failure.
  payments_network_interface().set_select_challenge_option_result(
      PaymentsRpcResult::kVcnRetrievalPermanentFailure);

  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the SelectChallengeRequest content.
  VerifySelectChallengeOptionRequest(
      /*context_token=*/"context_token_from_previous_unmask_response",
      kTestBillingCustomerNumber);
  // Verify error dialog is shown.
  EXPECT_TRUE(payments_autofill_client().autofill_error_dialog_shown());
  // Ensure the OTP authenticator is reset.
  EXPECT_TRUE(OtpAuthenticatorContextToken().empty());
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_FALSE(*(requester_->did_succeed()));
}

TEST_P(CreditCardOtpAuthenticatorTest,
       SelectChallengeOptionFailsWithVcnErrorMetrics) {
  base::HistogramTester histogram_tester;
  // Simulate server returns virtual card permanent failure.
  payments_network_interface().set_select_challenge_option_result(
      payments::PaymentsAutofillClient::PaymentsRpcResult::
          kVcnRetrievalPermanentFailure);
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);

  // Ensures the metrics have been logged correctly.
  std::string base_histogram_name =
      "Autofill.OtpAuth." + GetCardType() + "." + GetOtpAuthType();
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".Attempt", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result",
      autofill_metrics::OtpAuthEvent::
          kSelectedChallengeOptionVirtualCardRetrievalError,
      1);
  histogram_tester.ExpectTotalCount(
      base_histogram_name + ".RequestLatency.SelectChallengeOptionRequest", 1);
}

TEST_P(CreditCardOtpAuthenticatorTest,
       SelectChallengeOptionFailsWithOtherErrors) {
  // Simulate server returns non-virtual card permanent failure, e.g. response
  // not complete.
  payments_network_interface().set_select_challenge_option_result(
      PaymentsRpcResult::kPermanentFailure);

  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the SelectChallengeRequest content.
  VerifySelectChallengeOptionRequest(
      /*context_token=*/"context_token_from_previous_unmask_response",
      kTestBillingCustomerNumber);
  // Verify error dialog is shown.
  EXPECT_TRUE(payments_autofill_client().autofill_error_dialog_shown());
  // Ensure the OTP authenticator is reset.
  EXPECT_TRUE(OtpAuthenticatorContextToken().empty());
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_FALSE(*(requester_->did_succeed()));
}

TEST_P(CreditCardOtpAuthenticatorTest,
       SelectChallengeOptionFailsWithOtherErrorsMetrics) {
  base::HistogramTester histogram_tester;
  // Simulate server returns non-virtual card permanent failure, e.g. response
  // not complete.
  payments_network_interface().set_select_challenge_option_result(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure);

  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);

  // Ensures the metrics have been logged correctly.
  std::string base_histogram_name =
      "Autofill.OtpAuth." + GetCardType() + "." + GetOtpAuthType();
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".Attempt", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result",
      autofill_metrics::OtpAuthEvent::kSelectedChallengeOptionGenericError, 1);
  histogram_tester.ExpectTotalCount(
      base_histogram_name + ".RequestLatency.SelectChallengeOptionRequest", 1);
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthServerVcnError) {
  for (bool server_returned_decline_details : {true, false}) {
    // Simulate user selects OTP challenge option. Current context_token is from
    // previous unmask response. TestPaymentsNetworkInterface will ack the
    // select challenge option request and directly invoke the callback.
    authenticator_->OnChallengeOptionSelected(
        &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
        /*context_token=*/"context_token_from_previous_unmask_response",
        /*billing_customer_number=*/kTestBillingCustomerNumber);
    // Verify the context token is updated with SelectChallengeOption response.
    EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
    EXPECT_NE(OtpAuthenticatorContextToken(),
              "context_token_from_previous_unmask_response");
    EXPECT_TRUE(payments_autofill_client().show_otp_input_dialog());

    // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
    // TestPaymentsNetworkInterface just stores the unmask request detail, won't
    // invoke the callback. OnDidGetRealPan below will manually invoke the
    // callback.
    authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");
    // Simulate server returns virtual card retrieval try again failure. We will
    // show the error dialog and end session.
    OnDidGetRealPan(PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
                    /*real_pan=*/"", server_returned_decline_details);
    // Verify error dialog is shown.
    EXPECT_TRUE(payments_autofill_client().autofill_error_dialog_shown());
    if (server_returned_decline_details) {
      AutofillErrorDialogContext context =
          payments_autofill_client().autofill_error_dialog_context();
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
  }
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthServerVcnErrorMetrics) {
  for (bool server_returned_decline_details : {true, false}) {
    base::HistogramTester histogram_tester;
    // Simulate user selects OTP challenge option. Current context_token is from
    // previous unmask response. TestPaymentsNetworkInterface will ack the
    // select challenge option request and directly invoke the callback.
    authenticator_->OnChallengeOptionSelected(
        &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
        /*context_token=*/"context_token_from_previous_unmask_response",
        /*billing_customer_number=*/kTestBillingCustomerNumber);
    // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
    // TestPaymentsNetworkInterface just stores the unmask request detail, won't
    // invoke the callback. OnDidGetRealPan below will manually invoke the
    // callback.
    authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");
    // Simulate server returns virtual card retrieval try again failure. We will
    // show the error dialog and end session.
    OnDidGetRealPan(payments::PaymentsAutofillClient::PaymentsRpcResult::
                        kVcnRetrievalTryAgainFailure,
                    /*real_pan=*/"", server_returned_decline_details);

    // Ensures the metrics have been logged correctly.
    std::string base_histogram_name =
        "Autofill.OtpAuth." + GetCardType() + "." + GetOtpAuthType();
    histogram_tester.ExpectUniqueSample(base_histogram_name + ".Attempt", true,
                                        1);
    histogram_tester.ExpectUniqueSample(
        base_histogram_name + ".Result",
        autofill_metrics::OtpAuthEvent::kUnmaskCardVirtualCardRetrievalError,
        1);
    histogram_tester.ExpectTotalCount(
        base_histogram_name + ".RequestLatency.UnmaskCardRequest", 1);
    histogram_tester.ExpectTotalCount(
        base_histogram_name + ".RequestLatency.SelectChallengeOptionRequest",
        1);
  }
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthServerNonVcnError) {
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the context token is updated with SelectChallengeOption response.
  EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
  EXPECT_NE(OtpAuthenticatorContextToken(),
            "context_token_from_previous_unmask_response");
  EXPECT_TRUE(payments_autofill_client().show_otp_input_dialog());

  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsNetworkInterface just stores the unmask request detail, won't
  // invoke the callback. OnDidGetRealPan below will manually invoke the
  // callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");
  // Simulate server returns non-Vcn try again failure. We will reuse virtual
  // card error dialog and end session.
  OnDidGetRealPan(PaymentsRpcResult::kTryAgainFailure,
                  /*real_pan=*/"");
  // Verify error dialog is shown.
  EXPECT_TRUE(payments_autofill_client().autofill_error_dialog_shown());
  // Ensure the OTP authenticator is reset.
  EXPECT_TRUE(OtpAuthenticatorContextToken().empty());
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_FALSE(*(requester_->did_succeed()));
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthServerNonVcnErrorMetrics) {
  base::HistogramTester histogram_tester;
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);

  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsNetworkInterface just stores the unmask request detail, won't
  // invoke the callback. OnDidGetRealPan below will manually invoke the
  // callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");
  // Simulate server returns non-Vcn try again failure. We will reuse virtual
  // card error dialog and end session.
  OnDidGetRealPan(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kTryAgainFailure,
      /*real_pan=*/"");

  // Ensures the metrics have been logged correctly.
  std::string base_histogram_name =
      "Autofill.OtpAuth." + GetCardType() + "." + GetOtpAuthType();
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".Attempt", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result",
      autofill_metrics::OtpAuthEvent::kUnmaskCardAuthError, 1);
  histogram_tester.ExpectTotalCount(
      base_histogram_name + ".RequestLatency.UnmaskCardRequest", 1);
  histogram_tester.ExpectTotalCount(
      base_histogram_name + ".RequestLatency.SelectChallengeOptionRequest", 1);
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthMismatchThenRetry) {
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the context token is updated with SelectChallengeOption response.
  EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
  EXPECT_NE(OtpAuthenticatorContextToken(),
            "context_token_from_previous_unmask_response");
  EXPECT_TRUE(payments_autofill_client().show_otp_input_dialog());
  payments_autofill_client().ResetShowOtpInputDialog();

  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsNetworkInterface just stores the unmask request detail, won't
  // invoke the callback. OnDidGetRealPan below will manually invoke the
  // callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"222222");
  // Verify the selected challenge option is correctly set in
  // UnmaskRequestDetails.
  EXPECT_EQ(payments_network_interface()
                .unmask_request()
                ->selected_challenge_option->id,
            selected_otp_challenge_option_.id);
  // Verify that the otp is correctly set in UnmaskRequestDetails.
  EXPECT_EQ(payments_network_interface().unmask_request()->otp, u"222222");
  // Also verify that risk data is set in UnmaskRequestDetails.
  EXPECT_FALSE(
      payments_network_interface().unmask_request()->risk_data.empty());

  // Simulate otp mismatch, server returns flow_status indicating incorrect otp.
  OnDidGetRealPanWithFlowStatus(
      /*flow_status=*/"FLOW_STATUS_INCORRECT_OTP",
      /*context_token=*/"context_token_from_incorrect_otp");
  // Verify the context token is updated with unmask response.
  EXPECT_EQ(OtpAuthenticatorContextToken(), "context_token_from_incorrect_otp");

  // Simulate user types in another otp and click 'Confirm' again.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"333333");
  // Verify that the new otp is correctly set in UnmaskRequestDetails together
  // with the latest context token.
  EXPECT_EQ(payments_network_interface().unmask_request()->otp, u"333333");
  EXPECT_EQ(payments_network_interface().unmask_request()->context_token,
            "context_token_from_incorrect_otp");
  // Also verify that risk data is still set in UnmaskRequestDetails.
  EXPECT_FALSE(
      payments_network_interface().unmask_request()->risk_data.empty());

  // Simulate server returns success for the second try and invoke the callback.
  OnDidGetRealPan(PaymentsRpcResult::kSuccess, kTestNumber);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_TRUE(*(requester_->did_succeed()));
  EXPECT_EQ(kTestNumber16, requester_->number());
  EXPECT_EQ(GetUnmaskedCardRecordType(), requester_->record_type());
  EXPECT_FALSE(payments_autofill_client().show_otp_input_dialog());
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthMismatchThenRetryMetrics) {
  base::HistogramTester histogram_tester;
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  payments_autofill_client().ResetShowOtpInputDialog();
  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsNetworkInterface just stores the unmask request detail, won't
  // invoke the callback. OnDidGetRealPan below will manually invoke the
  // callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"222222");
  // Simulate otp mismatch, server returns flow_status indicating incorrect otp.
  OnDidGetRealPanWithFlowStatus(
      /*flow_status=*/"FLOW_STATUS_INCORRECT_OTP",
      /*context_token=*/"context_token_from_incorrect_otp");
  // Simulate user types in another otp and click 'Confirm' again.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"333333");
  // Simulate server returns success for the second try and invoke the callback.
  OnDidGetRealPan(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                  kTestNumber);

  // Ensures the metrics have been logged correctly.
  std::string base_histogram_name =
      "Autofill.OtpAuth." + GetCardType() + "." + GetOtpAuthType();
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".Attempt", true,
                                      1);
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".Result",
                                      autofill_metrics::OtpAuthEvent::kSuccess,
                                      1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".RetriableError",
      autofill_metrics::OtpAuthEvent::kOtpMismatch, 1);
  histogram_tester.ExpectTotalCount(
      base_histogram_name + ".RequestLatency.UnmaskCardRequest", 2);
  histogram_tester.ExpectTotalCount(
      base_histogram_name + ".RequestLatency.SelectChallengeOptionRequest", 1);
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthExpiredThenResendOtp) {
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the SelectChallengeRequest content.
  VerifySelectChallengeOptionRequest(
      /*context_token=*/"context_token_from_previous_unmask_response",
      kTestBillingCustomerNumber);
  // Verify the context token is updated with SelectChallengeOption response.
  EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
  EXPECT_NE(OtpAuthenticatorContextToken(),
            "context_token_from_previous_unmask_response");
  EXPECT_TRUE(payments_autofill_client().show_otp_input_dialog());
  payments_autofill_client().ResetShowOtpInputDialog();

  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsNetworkInterface just stores the unmask request detail, won't
  // invoke the callback. OnDidGetRealPan below will manually invoke the
  // callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"4444444");
  // Verify the selected challenge option is correctly set in
  // UnmaskRequestDetails.
  EXPECT_EQ(payments_network_interface()
                .unmask_request()
                ->selected_challenge_option->id,
            selected_otp_challenge_option_.id);
  // Verify that the otp is correctly set in UnmaskRequestDetails.
  EXPECT_EQ(payments_network_interface().unmask_request()->otp, u"4444444");
  // Also verify that risk data is set in UnmaskRequestDetails.
  EXPECT_FALSE(
      payments_network_interface().unmask_request()->risk_data.empty());

  // Simulate otp expired, server returns flow_status indicating expired otp.
  OnDidGetRealPanWithFlowStatus(
      /*flow_status=*/"FLOW_STATUS_EXPIRED_OTP",
      /*context_token=*/"context_token_from_expired_otp");
  // Verify the context token is updated with unmask response.
  EXPECT_EQ(OtpAuthenticatorContextToken(), "context_token_from_expired_otp");

  // Simulate user clicks "Get new code" from the UI, which calls
  // SendSelectChallengeOptionRequest() again. This will send the same selected
  // challenge option with the new context token.
  authenticator_->OnNewOtpRequested();
  // Verify the second SelectChallengeRequest is correctly set, the only
  // difference from the previous call is the context_token.
  VerifySelectChallengeOptionRequest(
      /*context_token=*/"context_token_from_expired_otp",
      kTestBillingCustomerNumber);
  EXPECT_FALSE(payments_autofill_client().show_otp_input_dialog());

  // Simulate user receives the new otp and types in the new otp.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"555555");
  // Verify that the new otp is correctly set in UnmaskRequestDetails together
  // with the latest context token.
  EXPECT_EQ(payments_network_interface().unmask_request()->otp, u"555555");
  // Note here is NOT EQUAL. The context token is from the new select challenge
  // option response.
  EXPECT_NE(payments_network_interface().unmask_request()->context_token,
            "context_token_from_expired_otp");
  // Also verify that risk data is still set in UnmaskRequestDetails.
  EXPECT_FALSE(
      payments_network_interface().unmask_request()->risk_data.empty());

  // Simulate server returns success for the second try and invoke the callback.
  OnDidGetRealPan(PaymentsRpcResult::kSuccess, kTestNumber);
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_TRUE(*(requester_->did_succeed()));
  EXPECT_EQ(kTestNumber16, requester_->number());
  EXPECT_EQ(GetUnmaskedCardRecordType(), requester_->record_type());
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthExpiredThenResendOtpMetrics) {
  base::HistogramTester histogram_tester;
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  payments_autofill_client().ResetShowOtpInputDialog();
  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsNetworkInterface just stores the unmask request detail, won't
  // invoke the callback. OnDidGetRealPan below will manually invoke the
  // callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"4444444");
  // Simulate otp expired, server returns flow_status indicating expired otp.
  OnDidGetRealPanWithFlowStatus(
      /*flow_status=*/"FLOW_STATUS_EXPIRED_OTP",
      /*context_token=*/"context_token_from_expired_otp");
  // Simulate user clicks "Get new code" from the UI, which calls
  // SendSelectChallengeOptionRequest() again. This will send the same selected
  // challenge option with the new context token.
  authenticator_->OnNewOtpRequested();
  // Simulate user receives the new otp and types in the new otp.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"555555");
  // Simulate server returns success for the second try and invoke the callback.
  OnDidGetRealPan(payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                  kTestNumber);

  // Ensures the metrics have been logged correctly.
  std::string base_histogram_name =
      "Autofill.OtpAuth." + GetCardType() + "." + GetOtpAuthType();
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".Attempt", true,
                                      1);
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".Result",
                                      autofill_metrics::OtpAuthEvent::kSuccess,
                                      1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".RetriableError",
      autofill_metrics::OtpAuthEvent::kOtpExpired, 1);
  histogram_tester.ExpectTotalCount(
      base_histogram_name + ".RequestLatency.UnmaskCardRequest", 2);
  histogram_tester.ExpectTotalCount(
      base_histogram_name + ".RequestLatency.SelectChallengeOptionRequest", 2);
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthCancelled) {
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the SelectChallengeRequest content.
  VerifySelectChallengeOptionRequest(
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
}

TEST_P(CreditCardOtpAuthenticatorTest, OtpAuthCancelledMetrics) {
  base::HistogramTester histogram_tester;
  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Simulate user closes the otp input dialog.
  authenticator_->OnUnmaskPromptClosed(/*user_closed_dialog=*/true);

  // Ensures the metrics have been logged correctly.
  std::string base_histogram_name =
      "Autofill.OtpAuth." + GetCardType() + "." + GetOtpAuthType();
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".Attempt", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result",
      autofill_metrics::OtpAuthEvent::kFlowCancelled, 1);
  histogram_tester.ExpectTotalCount(
      base_histogram_name + ".RequestLatency.UnmaskCardRequest", 0);
  histogram_tester.ExpectTotalCount(
      base_histogram_name + ".RequestLatency.SelectChallengeOptionRequest", 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CreditCardOtpAuthenticatorTest,
    // Skip kEmailOtp with kMaskedServerCard as it is not supported.
    testing::ValuesIn({
        std::make_tuple(CardUnmaskChallengeOptionType::kSmsOtp,
                        CreditCard::RecordType::kVirtualCard),
        std::make_tuple(CardUnmaskChallengeOptionType::kSmsOtp,
                        CreditCard::RecordType::kMaskedServerCard),
        std::make_tuple(CardUnmaskChallengeOptionType::kEmailOtp,
                        CreditCard::RecordType::kVirtualCard),
    }));

// CardInfoRetrieval currently only supports SmsOtp, hence setting up a
// separate SmsOtp error handling test for it. All other test cases are
// covered by the above parameterized tests.
class CreditCardOtpAuthenticatorCardInfoRetrievalErrorTest
    : public CreditCardOtpAuthenticatorTestBase {
 public:
  CreditCardOtpAuthenticatorCardInfoRetrievalErrorTest() = default;
  ~CreditCardOtpAuthenticatorCardInfoRetrievalErrorTest() override = default;

  void SetUp() override {
    CreditCardOtpAuthenticatorTestBase::SetUp();
    CreateSelectedOtpChallengeOption(CardUnmaskChallengeOptionType::kSmsOtp);
    card_.set_card_info_retrieval_enrollment_state(
        CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled);
  }
};

// Test failure of SelectChallenge option for cards enrolled in runtime
// retrieval.
TEST_F(CreditCardOtpAuthenticatorCardInfoRetrievalErrorTest,
       SelectChallengeOptionFailsWithCardInfoRetrievalError) {
  base::HistogramTester histogram_tester;
  // Simulate server returns card info retrieval permanent failure.
  payments_network_interface().set_select_challenge_option_result(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure);

  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  // Verify the SelectChallengeRequest content.
  VerifySelectChallengeOptionRequest(
      /*context_token=*/"context_token_from_previous_unmask_response",
      kTestBillingCustomerNumber);
  // Verify error dialog is shown.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());
  // Ensure the OTP authenticator is reset.
  EXPECT_TRUE(OtpAuthenticatorContextToken().empty());
  ASSERT_TRUE(requester_->did_succeed().has_value());
  EXPECT_FALSE(*(requester_->did_succeed()));

  // Ensures the metrics have been logged correctly.
  std::string base_histogram_name = "Autofill.OtpAuth.ServerCard.SmsOtp";
  histogram_tester.ExpectUniqueSample(base_histogram_name + ".Attempt", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      base_histogram_name + ".Result",
      autofill_metrics::OtpAuthEvent::kSelectedChallengeOptionGenericError, 1);
  histogram_tester.ExpectTotalCount(base_histogram_name +
                                        ".RequestLatency."
                                        "SelectChallengeOptionRequest",
                                    1);
}

// Server returns try again failure for cards enrolled in runtime retrieval
// after user enters the OTP.
TEST_F(CreditCardOtpAuthenticatorCardInfoRetrievalErrorTest,
       OtpAuthServerCardInfoRetrievalError) {
  for (bool server_returned_decline_details : {true, false}) {
    base::HistogramTester histogram_tester;
    // Simulate user selects OTP challenge option. Current context_token is from
    // previous unmask response. TestPaymentsNetworkInterface will ack the
    // select challenge option request and directly invoke the callback.
    authenticator_->OnChallengeOptionSelected(
        &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
        /*context_token=*/"context_token_from_previous_unmask_response",
        /*billing_customer_number=*/kTestBillingCustomerNumber);
    // Verify the context token is updated with SelectChallengeOption response.
    EXPECT_FALSE(OtpAuthenticatorContextToken().empty());
    EXPECT_NE(OtpAuthenticatorContextToken(),
              "context_token_from_previous_unmask_response");
    EXPECT_TRUE(
        autofill_client_.GetPaymentsAutofillClient()->show_otp_input_dialog());

    // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
    // TestPaymentsNetworkInterface just stores the unmask request detail, won't
    // invoke the callback. OnDidGetRealPan below will manually invoke the
    // callback.
    authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");
    // Simulate server returns card info retrieval try again failure. We will
    // show the error dialog and end session.
    OnDidGetRealPan(
        payments::PaymentsAutofillClient::PaymentsRpcResult::kTryAgainFailure,
        /*real_pan=*/"", server_returned_decline_details);
    // Verify error dialog is shown.
    EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                    ->autofill_error_dialog_shown());
    if (server_returned_decline_details) {
      AutofillErrorDialogContext context =
          autofill_client_.GetPaymentsAutofillClient()
              ->autofill_error_dialog_context();
      EXPECT_EQ(context.type,
                AutofillErrorDialogType::kCardInfoRetrievalTemporaryError);
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
        "Autofill.OtpAuth.ServerCard.SmsOtp.Attempt", true, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.OtpAuth.ServerCard.SmsOtp.Result",
        autofill_metrics::OtpAuthEvent::kUnmaskCardAuthError, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.OtpAuth.ServerCard.SmsOtp.RequestLatency.UnmaskCardRequest",
        1);
    histogram_tester.ExpectTotalCount(
        "Autofill.OtpAuth.ServerCard.SmsOtp.RequestLatency."
        "SelectChallengeOptionRequest",
        1);
  }
}

// Params of the CreditCardOtpAuthenticatorCardMetadataTest:
// -- bool card_name_available;
// -- bool card_art_available;
class CreditCardOtpAuthenticatorCardMetadataTest
    : public CreditCardOtpAuthenticatorTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  CreditCardOtpAuthenticatorCardMetadataTest() = default;
  ~CreditCardOtpAuthenticatorCardMetadataTest() override = default;

  void SetUp() override {
    CreditCardOtpAuthenticatorTestBase::SetUp();
    CreateSelectedOtpChallengeOption(CardUnmaskChallengeOptionType::kSmsOtp);
    card_.set_record_type(CreditCard::RecordType::kVirtualCard);
  }

  bool CardNameAvailable() { return std::get<0>(GetParam()); }
  bool CardArtAvailable() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(,
                         CreditCardOtpAuthenticatorCardMetadataTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(CreditCardOtpAuthenticatorCardMetadataTest, MetadataSignal) {
  if (CardNameAvailable()) {
    card_.set_product_description(u"fake product description");
  }
  if (CardArtAvailable()) {
    card_.set_card_art_url(GURL("https://www.example.com"));
  }

  // Simulate user selects OTP challenge option. Current context_token is from
  // previous unmask response. TestPaymentsNetworkInterface will ack the select
  // challenge option request and directly invoke the callback.
  authenticator_->OnChallengeOptionSelected(
      &card_, selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);

  // Simulate user provides the OTP and clicks 'Confirm' in the OTP dialog.
  // TestPaymentsNetworkInterface just stores the unmask request detail, won't
  // invoke the callback. OnDidGetRealPan below will manually invoke the
  // callback.
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");
  // Verify the selected challenge option is correctly set in
  // UnmaskRequestDetails.
  EXPECT_EQ(payments_network_interface()
                .unmask_request()
                ->selected_challenge_option->id,
            selected_otp_challenge_option_.id);
  // Verify that the otp is correctly set in UnmaskRequestDetails.
  EXPECT_EQ(payments_network_interface().unmask_request()->otp, u"111111");
  // Also verify that risk data is set in UnmaskRequestDetails.
  EXPECT_FALSE(
      payments_network_interface().unmask_request()->risk_data.empty());
  std::vector<ClientBehaviorConstants> signals =
      payments_network_interface().unmask_request()->client_behavior_signals;
  if (CardNameAvailable() && CardArtAvailable()) {
    EXPECT_NE(
        signals.end(),
        std::ranges::find(
            signals,
            ClientBehaviorConstants::kShowingCardArtImageAndCardProductName));
  } else {
    EXPECT_TRUE(signals.empty());
  }
}

// Params:
// 1. Function reference to call which creates the appropriate credit card
// benefit for the unittest.
// 2. Whether the flag to render benefits is enabled.
// 3. Whether the flag to sync benefits source is enabled.
// 4. Issuer ID which is set for the credit card with benefits.
// 5. Benefit source which is set for the credit card with benefits.
class CreditCardOtpAuthenticatorCardBenefitsTest
    : public CreditCardOtpAuthenticatorTestBase,
      public ::testing::WithParamInterface<
          std::tuple<base::FunctionRef<CreditCardBenefit()>,
                     bool,
                     bool,
                     std::string,
                     std::string>> {
 public:
  void SetUp() override {
    CreditCardOtpAuthenticatorTestBase::SetUp();
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kAutofillEnableCardBenefitsForAmericanExpress,
          IsCreditCardBenefitsEnabled()},
         {features::kAutofillEnableCardBenefitsForBmo,
          IsCreditCardBenefitsEnabled()},
         {features::kAutofillEnableFlatRateCardBenefitsFromCurinos,
          IsCreditCardBenefitsEnabled()},
         {features::kAutofillEnableCardBenefitsSourceSync,
          IsCreditCardBenefitsSourceSyncEnabled()}});
    CreateSelectedOtpChallengeOption(CardUnmaskChallengeOptionType::kSmsOtp);
    card_ = test::GetVirtualCard();
    autofill_client().set_last_committed_primary_main_frame_url(
        test::GetOriginsForMerchantBenefit().begin()->GetURL());
    if (IsCreditCardBenefitsSourceSyncEnabled()) {
      test::SetUpCreditCardAndBenefitData(
          card_, /*issuer_id=*/"", GetBenefit(), GetBenefitSource(),
          personal_data(), autofill_client().GetAutofillOptimizationGuide());
    } else {
      test::SetUpCreditCardAndBenefitData(
          card_, GetIssuerId(), GetBenefit(), /*benefit_source=*/"",
          personal_data(), autofill_client().GetAutofillOptimizationGuide());
    }
  }

  CreditCardBenefit GetBenefit() const { return std::get<0>(GetParam())(); }

  bool IsCreditCardBenefitsEnabled() const { return std::get<1>(GetParam()); }

  bool IsCreditCardBenefitsSourceSyncEnabled() const {
    return std::get<2>(GetParam());
  }

  const std::string& GetIssuerId() const { return std::get<3>(GetParam()); }

  const std::string& GetBenefitSource() const {
    return std::get<4>(GetParam());
  }

  bool ShouldShowCardBenefits() const {
    if (IsCreditCardBenefitsSourceSyncEnabled() &&
        GetBenefitSource() == "curinos") {
      return IsCreditCardBenefitsEnabled() &&
             std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit());
    }
    return IsCreditCardBenefitsEnabled();
  }

  const CreditCard& card() { return card_; }

 private:
  CreditCard card_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    CreditCardOtpAuthenticatorTestBase,
    CreditCardOtpAuthenticatorCardBenefitsTest,
    testing::Combine(
        ::testing::Values(&test::GetActiveCreditCardFlatRateBenefit,
                          &test::GetActiveCreditCardCategoryBenefit,
                          &test::GetActiveCreditCardMerchantBenefit),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Values("amex", "bmo"),
        ::testing::Values("amex", "bmo", "curinos")));

// Checks that ClientBehaviorConstants::kShowingCardBenefits is populated as a
// signal if a card benefit was shown when unmasking a credit card suggestion
// through the OTP authenticator.
TEST_P(CreditCardOtpAuthenticatorCardBenefitsTest,
       Benefits_ClientsBehaviorConstant) {
  authenticator_->OnChallengeOptionSelected(
      &card(), selected_otp_challenge_option_, requester_->GetWeakPtr(),
      /*context_token=*/"context_token_from_previous_unmask_response",
      /*billing_customer_number=*/kTestBillingCustomerNumber);
  authenticator_->OnUnmaskPromptAccepted(/*otp=*/u"111111");
  std::vector<ClientBehaviorConstants> signals =
      payments_network_interface().unmask_request()->client_behavior_signals;
  EXPECT_EQ(std::ranges::find(signals,
                              ClientBehaviorConstants::kShowingCardBenefits) !=
                signals.end(),
            ShouldShowCardBenefits());
}

}  // namespace
}  // namespace autofill
