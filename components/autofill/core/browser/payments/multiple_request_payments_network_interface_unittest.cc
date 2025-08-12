// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface.h"

#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface_test_base.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class MultipleRequestPaymentsNetworkInterfaceTest
    : public PaymentsNetworkInterfaceTestBase,
      public testing::Test {
 public:
  void SetUp() override {
    SetUpTest();
    payments_network_interface_ =
        std::make_unique<MultipleRequestPaymentsNetworkInterface>(
            test_shared_loader_factory_, *identity_test_env_.identity_manager(),
            /*is_off_the_record=*/false);
  }

 protected:
  // TODO: crbug.com/362787977 - After single request PaymentsNetworkInterface
  // is cleaned up, move this function to the
  // payments_network_interface_test_base.*.
  void ReturnResponse(int response_code, const std::string& response_body) {
    EXPECT_TRUE(
        payments_network_interface_->operations_for_testing().contains(id_));
    payments_network_interface_->operations_for_testing()
        .at(id_)
        ->OnSimpleLoaderCompleteInternalForTesting(response_code,
                                                   response_body);
  }

  std::unique_ptr<MultipleRequestPaymentsNetworkInterface>
      payments_network_interface_;
  MultipleRequestPaymentsNetworkInterface::RequestId id_;
};

class CreateCardTest : public MultipleRequestPaymentsNetworkInterfaceTest {
 public:
  CreateCardTest() = default;
  ~CreateCardTest() override = default;

 protected:
  void SendGetDetailsForCreateCardRequest() {
    UploadCardRequestDetails details;
    details.upload_card_source = UploadCardSource::kUpstreamSaveAndFill;
    details.client_behavior_signals = {};
    details.app_locale = "language-LOCALE";
    details.billing_customer_number = 111222333444L;
    details.profiles = {test::GetFullProfile(AddressCountryCode("US"))};

    id_ = payments_network_interface_->GetDetailsForCreateCard(
        details, base::BindOnce(&CreateCardTest::OnDidGetDetailsForCreateCard,
                                GetWeakPtr()));
  }

  void SendCreateCardRequest() {
    UploadCardRequestDetails request_details;
    request_details.billing_customer_number = 111122223333;
    request_details.card = test::GetCreditCard();
    request_details.cvc = u"123";
    request_details.card.SetNickname(u"some nickname");
    request_details.client_behavior_signals = {
        ClientBehaviorConstants::kOfferingToSaveCvc};
    request_details.context_token = u"some context token";
    request_details.risk_data = "some risk data";
    request_details.app_locale = "en";
    request_details.profiles.emplace_back(
        test::GetFullProfile(AddressCountryCode("US")));
    request_details.upload_card_source = UploadCardSource::kUpstreamSaveAndFill;

    id_ = payments_network_interface_->CreateCard(
        request_details,
        base::BindOnce(&CreateCardTest::OnDidCreateCard, GetWeakPtr()));
  }

  std::u16string context_token_;
  std::unique_ptr<base::Value::Dict> legal_message_;
  std::vector<std::pair<int, int>> supported_bin_ranges_;
  std::string instrument_id_;

 private:
  base::WeakPtr<CreateCardTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void OnDidGetDetailsForCreateCard(
      PaymentsAutofillClient::PaymentsRpcResult result,
      const std::u16string& context_token,
      std::unique_ptr<base::Value::Dict> legal_message,
      std::vector<std::pair<int, int>> supported_bin_ranges) {
    result_ = result;
    context_token_ = context_token;
    legal_message_ = std::move(legal_message);
    supported_bin_ranges_ = std::move(supported_bin_ranges);
  }

  void OnDidCreateCard(PaymentsAutofillClient::PaymentsRpcResult result,
                       const std::string& instrument_id) {
    result_ = result;
    instrument_id_ = instrument_id;
  }

  base::WeakPtrFactory<CreateCardTest> weak_ptr_factory_{this};
};

TEST_F(CreateCardTest, GetDetailsForCreateCard_Success) {
  SendGetDetailsForCreateCardRequest();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{"
                 "  \"context_token\" : \"some_token\","
                 "  \"legal_message\": {"
                 "    \"line\": ["
                 "      {\"template\": \"terms of service\"}]"
                 "  }, "
                 "  \"card_details\" : {\"supported_card_bin_ranges_string\": "
                 "\"1234,300000-555555,765\"}"
                 "}");

  EXPECT_EQ(PaymentsAutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ(u"some_token", context_token_);
  EXPECT_TRUE(legal_message_);
  EXPECT_EQ(3U, supported_bin_ranges_.size());
}

TEST_F(CreateCardTest, GetDetailsForCreateCard_Failure) {
  SendGetDetailsForCreateCardRequest();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"error\": { \"code\": \"INTERNAL\" }, \"context_token\": "
                 "\"some_token\", \"legal_message\": {} }");

  EXPECT_EQ(PaymentsAutofillClient::PaymentsRpcResult::kTryAgainFailure,
            result_);
}

TEST_F(CreateCardTest, CreateCardRequest_Success) {
  SendCreateCardRequest();
  IssueOAuthToken();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"card_info\": {\"instrument_id\": \"9223372036854775807\" } }");

  EXPECT_EQ(PaymentsAutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("9223372036854775807", instrument_id_);
}

TEST_F(CreateCardTest, CreateCardRequest_Failure) {
  SendCreateCardRequest();
  IssueOAuthToken();
  ReturnResponse(
      net::HTTP_OK,
      "{\"error\":{\"user_error_message\":\"Something went wrong!\"}}");

  EXPECT_EQ(PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure,
            result_);
}
typedef std::tuple<VirtualCardEnrollmentSource,
                   VirtualCardEnrollmentRequestType,
                   PaymentsRpcResult>
    UpdateVirtualCardEnrollmentTestData;

// TODO: crbug.com/403617982 - Update text name after the old
// PaymentsNetworkInterface is cleaned up.
class MultipleRequestUpdateVirtualCardEnrollmentTest
    : public MultipleRequestPaymentsNetworkInterfaceTest,
      public ::testing::WithParamInterface<
          UpdateVirtualCardEnrollmentTestData> {
 public:
  MultipleRequestUpdateVirtualCardEnrollmentTest() = default;
  ~MultipleRequestUpdateVirtualCardEnrollmentTest() override = default;

 protected:
  void TriggerFlow() {
    VirtualCardEnrollmentSource virtual_card_enrollment_source =
        std::get<0>(GetParam());
    VirtualCardEnrollmentRequestType virtual_card_enrollment_request_type =
        std::get<1>(GetParam());
    SendUpdateVirtualCardEnrollmentRequest(
        virtual_card_enrollment_source, virtual_card_enrollment_request_type);
    IssueOAuthToken();

    // `response_type_for_test` is the PaymentsRpcResult
    // response type we want to test for the combination of
    // `virtual_card_enrollment_source` and
    // `virtual_card_enrollment_request_type` we are currently on.
    PaymentsRpcResult response_type_for_test = std::get<2>(GetParam());
    switch (response_type_for_test) {
      case PaymentsRpcResult::kSuccess:
        if (virtual_card_enrollment_request_type ==
            VirtualCardEnrollmentRequestType::kEnroll) {
          ReturnResponse(net::HTTP_OK,
                         "{ \"enroll_result\": \"ENROLL_SUCCESS\" }");
        } else if (virtual_card_enrollment_request_type ==
                   VirtualCardEnrollmentRequestType::kUnenroll) {
          ReturnResponse(net::HTTP_OK, "{}");
        }
        break;
      case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
        ReturnResponse(
            net::HTTP_OK,
            "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
            "\"api_error_reason\": \"virtual_card_temporary_error\"} }");
        break;
      case PaymentsRpcResult::kTryAgainFailure:
        ReturnResponse(net::HTTP_OK,
                       "{ \"error\": { \"code\": \"INTERNAL\", "
                       "\"api_error_reason\": \"ANYTHING_ELSE\"} }");
        break;
      case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
        ReturnResponse(
            net::HTTP_OK,
            "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
            "\"api_error_reason\": \"virtual_card_permanent_error\"} }");
        break;
      case PaymentsRpcResult::kPermanentFailure:
        ReturnResponse(net::HTTP_OK,
                       "{ \"error\": { \"code\": \"ANYTHING_ELSE\" } }");
        break;
      case PaymentsRpcResult::kNetworkError:
        ReturnResponse(net::HTTP_REQUEST_TIMEOUT, "");
        break;
      case PaymentsRpcResult::kClientSideTimeout:
        ReturnResponse(net::ERR_TIMED_OUT, "");
        break;
      case PaymentsRpcResult::kNone:
        NOTREACHED();
    }
    EXPECT_EQ(response_type_for_test, result_);
  }

 private:
  base::WeakPtr<MultipleRequestUpdateVirtualCardEnrollmentTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SendUpdateVirtualCardEnrollmentRequest(
      VirtualCardEnrollmentSource virtual_card_enrollment_source,
      VirtualCardEnrollmentRequestType virtual_card_enrollment_request_type) {
    UpdateVirtualCardEnrollmentRequestDetails request_details;
    request_details.virtual_card_enrollment_request_type =
        virtual_card_enrollment_request_type;
    request_details.virtual_card_enrollment_source =
        virtual_card_enrollment_source;
    request_details.billing_customer_number = 555666777888;
    if (virtual_card_enrollment_request_type ==
        VirtualCardEnrollmentRequestType::kEnroll) {
      request_details.vcn_context_token = "fake context token";
    }
    request_details.instrument_id = 12345678;
    id_ = payments_network_interface_->UpdateVirtualCardEnrollment(
        request_details,
        base::BindOnce(&MultipleRequestUpdateVirtualCardEnrollmentTest::
                           OnDidUpdateVirtualCardEnrollmentResponse,
                       GetWeakPtr()));
  }

  void OnDidUpdateVirtualCardEnrollmentResponse(
      PaymentsAutofillClient::PaymentsRpcResult result) {
    result_ = result;
  }

  base::WeakPtrFactory<MultipleRequestUpdateVirtualCardEnrollmentTest>
      weak_ptr_factory_{this};
};

// Initializes the parameterized test suite with all possible values of
// VirtualCardEnrollmentSource, VirtualCardEnrollmentRequestType, and
// PaymentsRpcResult.
INSTANTIATE_TEST_SUITE_P(
    ,
    MultipleRequestUpdateVirtualCardEnrollmentTest,
    testing::Combine(
        testing::Values(VirtualCardEnrollmentSource::kUpstream,
                        VirtualCardEnrollmentSource::kDownstream,
                        VirtualCardEnrollmentSource::kSettingsPage),
        testing::Values(VirtualCardEnrollmentRequestType::kEnroll,
                        VirtualCardEnrollmentRequestType::kUnenroll),
        testing::Values(PaymentsRpcResult::kSuccess,
                        PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
                        PaymentsRpcResult::kTryAgainFailure,
                        PaymentsRpcResult::kVcnRetrievalPermanentFailure,
                        PaymentsRpcResult::kPermanentFailure,
                        PaymentsRpcResult::kNetworkError,
                        PaymentsRpcResult::kClientSideTimeout)));

// Parameterized test that tests all combinations of
// VirtualCardEnrollmentSource and VirtualCardEnrollmentRequestType against
// all possible server responses in the UpdateVirtualCardEnrollmentFlow. This
// test will be run once for each combination.
TEST_P(MultipleRequestUpdateVirtualCardEnrollmentTest,
       UpdateVirtualCardEnrollmentTest_TestAllFlows) {
  TriggerFlow();
}

class MultipleRequestGetVirtualCardEnrollmentDetailsTest
    : public MultipleRequestPaymentsNetworkInterfaceTest,
      public ::testing::WithParamInterface<
          std::tuple<VirtualCardEnrollmentSource, PaymentsRpcResult>> {
 public:
  MultipleRequestGetVirtualCardEnrollmentDetailsTest() = default;
  ~MultipleRequestGetVirtualCardEnrollmentDetailsTest() override = default;

  void OnDidGetVirtualCardEnrollmentDetails(
      PaymentsRpcResult result,
      const payments::GetDetailsForEnrollmentResponseDetails&
          get_details_for_enrollment_response_fields) {
    result_ = result;
    get_details_for_enrollment_response_fields_ =
        get_details_for_enrollment_response_fields;
  }

 protected:
  base::WeakPtr<MultipleRequestGetVirtualCardEnrollmentDetailsTest>
  GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  GetDetailsForEnrollmentResponseDetails
      get_details_for_enrollment_response_fields_;

  base::WeakPtrFactory<MultipleRequestGetVirtualCardEnrollmentDetailsTest>
      weak_ptr_factory_{this};
};

// Initializes the parameterized test suite with all possible combinations of
// VirtualCardEnrollmentSource and PaymentsRpcResult.
INSTANTIATE_TEST_SUITE_P(
    ,
    MultipleRequestGetVirtualCardEnrollmentDetailsTest,
    testing::Combine(
        testing::Values(VirtualCardEnrollmentSource::kUpstream,
                        VirtualCardEnrollmentSource::kDownstream,
                        VirtualCardEnrollmentSource::kSettingsPage),
        testing::Values(PaymentsRpcResult::kSuccess,
                        PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
                        PaymentsRpcResult::kTryAgainFailure,
                        PaymentsRpcResult::kVcnRetrievalPermanentFailure,
                        PaymentsRpcResult::kPermanentFailure,
                        PaymentsRpcResult::kNetworkError,
                        PaymentsRpcResult::kClientSideTimeout)));

// Parameterized test that tests all combinations of
// VirtualCardEnrollmentSource and server PaymentsRpcResult. This test
// will be run once for each combination.
TEST_P(MultipleRequestGetVirtualCardEnrollmentDetailsTest,
       GetVirtualCardEnrollmentDetailsTest_TestAllFlows) {
  GetDetailsForEnrollmentRequestDetails request_details;
  request_details.source = std::get<0>(GetParam());
  request_details.instrument_id = 12345678;
  request_details.billing_customer_number = 555666777888;
  request_details.risk_data = "fake risk data";
  request_details.app_locale = "en";

  id_ = payments_network_interface_->GetVirtualCardEnrollmentDetails(
      request_details,
      base::BindOnce(&MultipleRequestGetVirtualCardEnrollmentDetailsTest::
                         OnDidGetVirtualCardEnrollmentDetails,
                     GetWeakPtr()));
  IssueOAuthToken();

  // Ensures the PaymentsRpcResult is set correctly.
  PaymentsRpcResult result = std::get<1>(GetParam());
  switch (result) {
    case PaymentsRpcResult::kSuccess:
      ReturnResponse(
          net::HTTP_OK,
          "{ \"google_legal_message\": { \"line\" : [{ \"template\": \"This "
          "is the entire message.\" }] }, \"external_legal_message\": {}, "
          "\"context_token\": \"some_token\" }");
      break;
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
      ReturnResponse(
          net::HTTP_OK,
          "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
          "\"api_error_reason\": \"virtual_card_temporary_error\"} }");
      break;
    case PaymentsRpcResult::kTryAgainFailure:
      ReturnResponse(net::HTTP_OK,
                     "{ \"error\": { \"code\": \"INTERNAL\", "
                     "\"api_error_reason\": \"ANYTHING_ELSE\"} }");
      break;
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      ReturnResponse(
          net::HTTP_OK,
          "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
          "\"api_error_reason\": \"virtual_card_permanent_error\"} }");
      break;
    case PaymentsRpcResult::kPermanentFailure:
      ReturnResponse(net::HTTP_OK,
                     "{ \"error\": { \"code\": \"ANYTHING_ELSE\" } }");
      break;
    case PaymentsRpcResult::kNetworkError:
      ReturnResponse(net::HTTP_REQUEST_TIMEOUT, "");
      break;
    case PaymentsRpcResult::kClientSideTimeout:
      ReturnResponse(net::ERR_TIMED_OUT, "");
      break;
    case PaymentsRpcResult::kNone:
      NOTREACHED();
  }
  EXPECT_EQ(result, result_);
}

}  // namespace autofill::payments
