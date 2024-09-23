// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface_test_base.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_response_details.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

class FacilitatedPaymentsNetworkInterfaceTest
    : public autofill::payments::PaymentsNetworkInterfaceTestBase,
      public testing::Test {
 public:
  FacilitatedPaymentsNetworkInterfaceTest() = default;

  FacilitatedPaymentsNetworkInterfaceTest(
      const FacilitatedPaymentsNetworkInterfaceTest&) = delete;
  FacilitatedPaymentsNetworkInterfaceTest& operator=(
      const FacilitatedPaymentsNetworkInterfaceTest&) = delete;

  ~FacilitatedPaymentsNetworkInterfaceTest() override = default;

  void SetUp() override {
    SetUpTest();
    payments_network_interface_ =
        std::make_unique<FacilitatedPaymentsNetworkInterface>(
            test_shared_loader_factory_, identity_test_env_.identity_manager(),
            &test_personal_data_.payments_data_manager());
  }

  void TearDown() override { payments_network_interface_.reset(); }

 protected:
  void SendInitiatePaymentRequest() {
    auto request_details =
        std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>();
    request_details->risk_data_ = "seems pretty risky";
    request_details->client_token_ =
        std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
    request_details->billing_customer_number_ = 11;
    request_details->merchant_payment_page_hostname_ = "foo.com";
    request_details->instrument_id_ = 13;
    request_details->pix_code_ = "a valid code";
    payments_network_interface_->InitiatePayment(
        std::move(request_details),
        base::BindOnce(&FacilitatedPaymentsNetworkInterfaceTest::
                           OnInitiatePaymentResponseReceived,
                       GetWeakPtr()),
        "language-LOCALE");
  }

  std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
      response_details_;
  std::unique_ptr<FacilitatedPaymentsNetworkInterface>
      payments_network_interface_;

 private:
  void OnInitiatePaymentResponseReceived(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
      std::unique_ptr<FacilitatedPaymentsInitiatePaymentResponseDetails>
          response_details) {
    result_ = result;
    response_details_ = std::move(response_details);
  }

  base::WeakPtr<FacilitatedPaymentsNetworkInterfaceTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<FacilitatedPaymentsNetworkInterfaceTest>
      weak_ptr_factory_{this};
};

TEST_F(FacilitatedPaymentsNetworkInterfaceTest,
       InitiatePaymentRequest_Success) {
  SendInitiatePaymentRequest();
  IssueOAuthToken();
  ReturnResponse(
      payments_network_interface_.get(), net::HTTP_OK,
      "{\"trigger_purchase_manager\":{\"o2_action_token\":\"dG9rZW4=\"}}");

  // Verify the request contains necessary info like the payment details, and
  // the instrument id.
  EXPECT_TRUE(GetUploadData().find("payment_details") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("sender_instrument_id") !=
              std::string::npos);

  // Verify that a success result was received because the response contained
  // the action token.
  EXPECT_EQ(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      result_);
  std::vector<uint8_t> expected_action_token = {'t', 'o', 'k', 'e', 'n'};
  EXPECT_EQ(expected_action_token, response_details_->action_token_);
}

TEST_F(FacilitatedPaymentsNetworkInterfaceTest,
       InitiatePaymentRequest_Failure) {
  SendInitiatePaymentRequest();
  IssueOAuthToken();
  ReturnResponse(
      payments_network_interface_.get(), net::HTTP_OK,
      "{\"error\":{\"user_error_message\":\"Something went wrong!\"}}");

  // Verify the request contains necessary info like the payment details, and
  // the instrument id.
  EXPECT_TRUE(GetUploadData().find("payment_details") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("sender_instrument_id") !=
              std::string::npos);

  // Verify that a failure result was received because the response contained
  // error.
  EXPECT_EQ(autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
                kPermanentFailure,
            result_);
  EXPECT_EQ("Something went wrong!", response_details_->error_message_.value());
}

}  // namespace payments::facilitated
