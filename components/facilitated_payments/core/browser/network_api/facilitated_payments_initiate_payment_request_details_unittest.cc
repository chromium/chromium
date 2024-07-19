// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments::facilitated {

class FacilitatedPaymentsInitiatePaymentRequestDetailsTest
    : public testing::Test {
 public:
  void SetUp() override {
    request_details_ =
        std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>();
  }

 protected:
  std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
      request_details_;
};

TEST_F(FacilitatedPaymentsInitiatePaymentRequestDetailsTest,
       NoRiskData_IsReadyForPixPaymentReturnsFalse) {
  request_details_->client_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  request_details_->billing_customer_number_ = 13;
  request_details_->merchant_payment_page_hostname_ = "foo.com";
  request_details_->instrument_id_ = 13;
  request_details_->pix_code_ = "a valid code";

  EXPECT_FALSE(request_details_->IsReadyForPixPayment());
}

TEST_F(FacilitatedPaymentsInitiatePaymentRequestDetailsTest,
       NoClientToken_IsReadyForPixPaymentReturnsFalse) {
  request_details_->risk_data_ = "seems pretty risky";
  request_details_->billing_customer_number_ = 13;
  request_details_->merchant_payment_page_hostname_ = "foo.com";
  request_details_->instrument_id_ = 13;
  request_details_->pix_code_ = "a valid code";

  EXPECT_FALSE(request_details_->IsReadyForPixPayment());
}

TEST_F(FacilitatedPaymentsInitiatePaymentRequestDetailsTest,
       NoBillingCustomerNumber_IsReadyForPixPaymentReturnsFalse) {
  request_details_->risk_data_ = "seems pretty risky";
  request_details_->client_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  request_details_->merchant_payment_page_hostname_ = "foo.com";
  request_details_->instrument_id_ = 13;
  request_details_->pix_code_ = "a valid code";

  EXPECT_FALSE(request_details_->IsReadyForPixPayment());
}

TEST_F(FacilitatedPaymentsInitiatePaymentRequestDetailsTest,
       NoMerchantPaymentPageUrl_IsReadyForPixPaymentReturnsFalse) {
  request_details_->risk_data_ = "seems pretty risky";
  request_details_->client_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  request_details_->billing_customer_number_ = 13;
  request_details_->instrument_id_ = 13;
  request_details_->pix_code_ = "a valid code";

  EXPECT_FALSE(request_details_->IsReadyForPixPayment());
}

TEST_F(FacilitatedPaymentsInitiatePaymentRequestDetailsTest,
       NoInstrumentId_IsReadyForPixPaymentReturnsFalse) {
  request_details_->risk_data_ = "seems pretty risky";
  request_details_->client_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  request_details_->billing_customer_number_ = 13;
  request_details_->merchant_payment_page_hostname_ = "foo.com";
  request_details_->pix_code_ = "a valid code";

  EXPECT_FALSE(request_details_->IsReadyForPixPayment());
}

TEST_F(FacilitatedPaymentsInitiatePaymentRequestDetailsTest,
       NoPixCode_IsReadyForPixPaymentReturnsFalse) {
  request_details_->client_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  request_details_->risk_data_ = "seems pretty risky";
  request_details_->billing_customer_number_ = 13;
  request_details_->merchant_payment_page_hostname_ = "foo.com";
  request_details_->instrument_id_ = 13;

  EXPECT_FALSE(request_details_->IsReadyForPixPayment());
}

TEST_F(FacilitatedPaymentsInitiatePaymentRequestDetailsTest,
       AllPixRequestDetailsAvailable_IsReadyForPixPaymentReturnsTrue) {
  request_details_->risk_data_ = "seems pretty risky";
  request_details_->client_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  request_details_->billing_customer_number_ = 13;
  request_details_->merchant_payment_page_hostname_ = "foo.com";
  request_details_->instrument_id_ = 13;
  request_details_->pix_code_ = "a valid code";

  EXPECT_TRUE(request_details_->IsReadyForPixPayment());
}

}  // namespace payments::facilitated
