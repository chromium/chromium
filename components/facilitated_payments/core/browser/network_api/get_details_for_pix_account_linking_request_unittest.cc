// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/network_api/get_details_for_pix_account_linking_request.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

using GetDetailsForPixAccountLinkingRequestTest = testing::Test;

TEST_F(GetDetailsForPixAccountLinkingRequestTest, VerifyRequestContent) {
  auto request = std::make_unique<GetDetailsForPixAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true);

  EXPECT_EQ(request->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/"
            "getdetailsforcreatepaymentinstrument");
  EXPECT_EQ(request->GetRequestContentType(), "application/json");
  // Verify that all the data is added to the request content.
  EXPECT_EQ(request->GetRequestContent(),
            "{\"chrome_user_context\":{\"full_sync_enabled\":true},\"context\":"
            "{\"billable_service\":70073,\"customer_context\":{\"external_"
            "customer_id\":\"123\"},\"language_code\":\"US\"},\"pix_account_"
            "linking_info\":{}}");
}

TEST_F(GetDetailsForPixAccountLinkingRequestTest,
       VerifyRequestContent_WithClientToken) {
  auto request = std::make_unique<GetDetailsForPixAccountLinkingRequest>(
      123, std::vector<uint8_t>{'a', 'b', 'c'},
      /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true);

  EXPECT_EQ(request->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/"
            "getdetailsforcreatepaymentinstrument");
  EXPECT_EQ(request->GetRequestContentType(), "application/json");
  EXPECT_EQ(request->GetRequestContent(),
            "{\"chrome_user_context\":{\"full_sync_enabled\":true},\"client_"
            "token\":\"YWJj\","
            "\"context\":{\"billable_service\":70073,\"customer_context\":{"
            "\"external_"
            "customer_id\":\"123\"},\"language_code\":\"US\"},\"pix_account_"
            "linking_info\":{}}");
}

TEST_F(GetDetailsForPixAccountLinkingRequestTest,
       ParseResponse_Success_AccountLinkingEligibilitySetToTrue) {
  auto request = std::make_unique<GetDetailsForPixAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true);
  std::optional<base::Value> response = base::JSONReader::Read(
      R"({"pix_account_linking_details":{"action_token":"YWJj"}})",
      base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());

  EXPECT_TRUE(request->IsResponseComplete());
  EXPECT_TRUE(request->is_eligible_for_pix_account_linking_);
}

TEST_F(GetDetailsForPixAccountLinkingRequestTest,
       ParseResponse_SuccessWithActionToken) {
  std::vector<uint8_t> result_action_token;
  auto request = std::make_unique<GetDetailsForPixAccountLinkingRequest>(
      123, std::vector<uint8_t>{},
      base::BindOnce(
          [](std::vector<uint8_t>* out_token,
             autofill::payments::PaymentsAutofillClient::PaymentsRpcResult
                 result,
             bool is_eligible, const std::vector<uint8_t>& action_token) {
            *out_token = action_token;
          },
          &result_action_token),
      /*app_locale=*/"US", /*full_sync_enabled=*/true);

  // "YWJj" is Base64 for "abc" ({'a', 'b', 'c'})
  std::optional<base::Value> response = base::JSONReader::Read(
      R"({"pix_account_linking_details":{"action_token":"YWJj"}})",
      base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());
  request->RespondToDelegate(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);

  EXPECT_TRUE(request->is_eligible_for_pix_account_linking_);
  EXPECT_EQ(result_action_token, (std::vector<uint8_t>{'a', 'b', 'c'}));
}

TEST_F(
    GetDetailsForPixAccountLinkingRequestTest,
    ParseResponse_SuccessWithoutActionToken_AccountLinkingEligibilitySetToFalse) {
  std::vector<uint8_t> result_action_token;
  auto request = std::make_unique<GetDetailsForPixAccountLinkingRequest>(
      123, std::vector<uint8_t>{},
      base::BindOnce(
          [](std::vector<uint8_t>* out_token,
             autofill::payments::PaymentsAutofillClient::PaymentsRpcResult
                 result,
             bool is_eligible, const std::vector<uint8_t>& action_token) {
            *out_token = action_token;
          },
          &result_action_token),
      /*app_locale=*/"US", /*full_sync_enabled=*/true);

  std::optional<base::Value> response =
      base::JSONReader::Read(R"({"pix_account_linking_details":{}})",
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());
  request->RespondToDelegate(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);

  EXPECT_FALSE(request->is_eligible_for_pix_account_linking_);
  EXPECT_TRUE(result_action_token.empty());
}

TEST_F(
    GetDetailsForPixAccountLinkingRequestTest,
    ParseResponse_SuccessWithoutPixAccountLinkingDetails_AccountLinkingEligibilitySetToFalse) {
  auto request = std::make_unique<GetDetailsForPixAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true);
  std::optional<base::Value> response =
      base::JSONReader::Read("{}", base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());

  EXPECT_TRUE(request->IsResponseComplete());
  EXPECT_FALSE(request->is_eligible_for_pix_account_linking_);
}

TEST_F(GetDetailsForPixAccountLinkingRequestTest, ParseResponse_Error) {
  auto request = std::make_unique<GetDetailsForPixAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true);
  std::optional<base::Value> response = base::JSONReader::Read(
      "{\"error\":\"error\"}", base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());

  EXPECT_TRUE(request->IsResponseComplete());
  EXPECT_FALSE(request->is_eligible_for_pix_account_linking_);
}

}  // namespace payments::facilitated
