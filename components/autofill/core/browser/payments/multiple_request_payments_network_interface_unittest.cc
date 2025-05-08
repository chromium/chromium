// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface.h"

#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface_test_base.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class CreateCardTest : public PaymentsNetworkInterfaceTestBase,
                       public testing::Test {
 public:
  CreateCardTest() = default;
  CreateCardTest(const CreateCardTest&) = delete;
  CreateCardTest& operator=(const CreateCardTest&) = delete;
  ~CreateCardTest() override = default;

  void SetUp() override {
    SetUpTest();
    payments_network_interface_ =
        std::make_unique<MultipleRequestPaymentsNetworkInterface>(
            test_shared_loader_factory_, *identity_test_env_.identity_manager(),
            /*is_off_the_record=*/false);
  }

  void TearDown() override { payments_network_interface_.reset(); }

 protected:
  void SendGetDetailsForCreateCardRequest() {
    id_ = payments_network_interface_->GetDetailsForCreateCard(
        "US",
        /*client_behavior_signals=*/{}, "language-LOCALE",
        base::BindOnce(&CreateCardTest::OnDidGetDetailsForCreateCard,
                       GetWeakPtr()),
        /*billable_service_number=*/12345,
        /*billing_customer_number=*/111222333444L,
        /*upload_card_source=*/
        UploadCardSource::UPSTREAM_SAVE_AND_FILL);
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
    request_details.upload_card_source =
        UploadCardSource::UPSTREAM_SAVE_AND_FILL;

    id_ = payments_network_interface_->CreateCard(
        request_details,
        base::BindOnce(&CreateCardTest::OnDidCreateCard, GetWeakPtr()));
  }

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

  MultipleRequestPaymentsNetworkInterface::RequestId id_;

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

}  // namespace autofill::payments
