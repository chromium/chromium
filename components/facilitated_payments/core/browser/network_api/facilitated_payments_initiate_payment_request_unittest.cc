// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request.h"

#include <memory>

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments::facilitated {

using FacilitatedPaymentsInitiatePaymentRequestTest = testing::Test;

TEST_F(FacilitatedPaymentsInitiatePaymentRequestTest,
       RequestContents_WithAllDetails) {
  auto request_details_full =
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>();
  request_details_full->risk_data_ = "seems pretty risky";
  // The client token will be base64 encoded as "dG9rZW4=" in the request
  // content.
  request_details_full->client_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  request_details_full->billing_customer_number_ = 11;
  request_details_full->merchant_payment_page_hostname_ = "foo.com";
  request_details_full->instrument_id_ = 13;
  request_details_full->pix_code_ = "a valid code";

  auto request = std::make_unique<FacilitatedPaymentsInitiatePaymentRequest>(
      std::move(request_details_full), /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true);

  EXPECT_EQ(request->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/initiatepayment");
  EXPECT_EQ(request->GetRequestContentType(), "application/json");
  //   Verify that all the data is added to the request content.
  EXPECT_EQ(
      request->GetRequestContent(),
      "{\"chrome_user_context\":{\"full_sync_enabled\":true},\"client_token\":"
      "\"dG9rZW4=\",\"context\":{\"billable_service\":70154,\"customer_"
      "context\":{"
      "\"external_customer_id\":\"11\"},\"language_code\":\"US\"},\"merchant_"
      "info\":{\"merchant_checkout_page_url\":\"foo.com\"},\"payment_details\":"
      "{\"payment_rail\":\"PIX\",\"qr_code\":\"a "
      "valid "
      "code\"},\"risk_data_encoded\":{\"encoding_type\":\"BASE_64\",\"message_"
      "type\":\"BROWSER_NATIVE_FINGERPRINTING\",\"value\":\"seems pretty "
      "risky\"},\"sender_instrument_id\":\"13\"}");
}

TEST_F(FacilitatedPaymentsInitiatePaymentRequestTest,
       RequestContents_WithoutOptionalDetails) {
  auto request_details_without_optional_data =
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>();
  // `billing_customer_number_` and `merchant_payment_page_hostname_` optional
  // fields are not set.
  request_details_without_optional_data->risk_data_ = "seems pretty risky";
  // The client token will be base64 encoded as "dG9rZW4=" in the request
  // content.
  request_details_without_optional_data->client_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  request_details_without_optional_data->instrument_id_ = 13;
  request_details_without_optional_data->pix_code_ = "a valid code";

  auto request = std::make_unique<FacilitatedPaymentsInitiatePaymentRequest>(
      std::move(request_details_without_optional_data),
      /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true);

  EXPECT_EQ(request->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/initiatepayment");
  EXPECT_EQ(request->GetRequestContentType(), "application/json");
  // Verify that all data except the optional fields are added to the request
  // content. Excluded field `billing_customer_number_` maps to
  // `external_customer_id`, and `merchant_payment_page_hostname_` maps to
  // `merchant_checkout_page_url` in the request content. Both these data should
  // be absent.
  EXPECT_EQ(request->GetRequestContent(),
            "{\"chrome_user_context\":{\"full_sync_enabled\":true},\"client_"
            "token\":\"dG9rZW4=\",\"context\":{\"billable_service\":70154,"
            "\"language_code\":\"US\"},\"payment_details\":{\"payment_rail\":"
            "\"PIX\",\"qr_code\":\"a valid "
            "code\"},\"risk_data_encoded\":{\"encoding_type\":\"BASE_64\","
            "\"message_type\":\"BROWSER_NATIVE_FINGERPRINTING\",\"value\":"
            "\"seems pretty risky\"},\"sender_instrument_id\":\"13\"}");
}

TEST_F(FacilitatedPaymentsInitiatePaymentRequestTest,
       ParseResponse_WithActionToken) {
  auto request = std::make_unique<FacilitatedPaymentsInitiatePaymentRequest>(
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>(),
      /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true);
  // The action token "token" is base64 encoded as "dG9rZW4=" in the response
  // content.
  std::optional<base::Value> response = base::JSONReader::Read(
      "{\"trigger_purchase_manager\":{\"o2_action_token\":\"dG9rZW4=\"}}");
  request->ParseResponse(response->GetDict());

  std::vector<uint8_t> expected_action_token = {'t', 'o', 'k', 'e', 'n'};
  EXPECT_EQ(expected_action_token, request->response_details_->action_token_);

  // Verify that the response is considered complete.
  EXPECT_TRUE(request->IsResponseComplete());
}

TEST_F(FacilitatedPaymentsInitiatePaymentRequestTest,
       ParseResponse_WithCorruptActionToken) {
  auto request = std::make_unique<FacilitatedPaymentsInitiatePaymentRequest>(
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>(),
      /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true);
  // Set a corrupt base64 action token to simulate Base64Decode to return an
  // empty vector.
  std::optional<base::Value> response = base::JSONReader::Read(
      "{\"trigger_purchase_manager\":{\"o2_action_token\":\"dG9r00ZW4=\"}}");
  request->ParseResponse(response->GetDict());

  EXPECT_TRUE(request->response_details_->action_token_.empty());
  // Verify that the response is considered incomplete.
  EXPECT_FALSE(request->IsResponseComplete());
}

TEST_F(FacilitatedPaymentsInitiatePaymentRequestTest,
       ParseResponse_WithErrorMessage) {
  auto request = std::make_unique<FacilitatedPaymentsInitiatePaymentRequest>(
      std::make_unique<FacilitatedPaymentsInitiatePaymentRequestDetails>(),
      /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true);
  std::optional<base::Value> response = base::JSONReader::Read(
      "{\"error\":{\"user_error_message\":\"Something went wrong!\"}}");
  request->ParseResponse(response->GetDict());

  EXPECT_EQ("Something went wrong!",
            request->response_details_->error_message_.value());

  // Verify that the response is considered incomplete.
  EXPECT_FALSE(request->IsResponseComplete());
}

}  // namespace payments::facilitated
