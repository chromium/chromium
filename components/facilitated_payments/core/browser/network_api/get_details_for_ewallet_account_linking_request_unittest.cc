// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/network_api/get_details_for_ewallet_account_linking_request.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/test/gtest_util.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

class GetDetailsForEwalletAccountLinkingRequestTest : public testing::Test {
 protected:
  base::DictValue CreateSampleAccountLinkingPayload(
      const std::string& issuer_id = "touchngo") {
    return base::DictValue().Set("ewallet_account_linking_info",
                                 base::DictValue().Set("issuer_id", issuer_id));
  }
};

TEST_F(GetDetailsForEwalletAccountLinkingRequestTest, VerifyRequestContent) {
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true,
      /*account_linking_payload=*/CreateSampleAccountLinkingPayload());

  EXPECT_EQ(request->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/"
            "getdetailsforcreatepaymentinstrument");
  EXPECT_EQ(request->GetRequestContentType(), "application/json");
  // Verify that all default data is added to the request content.
  EXPECT_EQ(
      request->GetRequestContent(),
      "{\"chrome_user_context\":{\"full_sync_enabled\":true},\"context\":"
      "{\"billable_service\":70073,\"customer_context\":{\"external_"
      "customer_id\":\"123\"},\"language_code\":\"US\"},\"ewallet_account_"
      "linking_info\":{\"issuer_id\":\"touchngo\"}}");
}

TEST_F(GetDetailsForEwalletAccountLinkingRequestTest,
       VerifyRequestContent_WithCustomAccountLinkingPayload) {
  base::DictValue payload =
      base::DictValue().Set("ewallet_account_linking_info",
                            base::DictValue().Set("issuer_id", "shopeepay"));
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true, std::move(payload));

  EXPECT_EQ(
      request->GetRequestContent(),
      "{\"chrome_user_context\":{\"full_sync_enabled\":true},\"context\":"
      "{\"billable_service\":70073,\"customer_context\":{\"external_"
      "customer_id\":\"123\"},\"language_code\":\"US\"},\"ewallet_account_"
      "linking_info\":{\"issuer_id\":\"shopeepay\"}}");
}

TEST_F(GetDetailsForEwalletAccountLinkingRequestTest,
       VerifyRequestContent_WithClientToken) {
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{'a', 'b', 'c'},
      /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true,
      /*account_linking_payload=*/CreateSampleAccountLinkingPayload());

  EXPECT_EQ(request->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/"
            "getdetailsforcreatepaymentinstrument");
  EXPECT_EQ(request->GetRequestContentType(), "application/json");
  EXPECT_EQ(
      request->GetRequestContent(),
      "{\"chrome_user_context\":{\"full_sync_enabled\":true},\"client_"
      "token\":\"YWJj\","
      "\"context\":{\"billable_service\":70073,\"customer_context\":{"
      "\"external_"
      "customer_id\":\"123\"},\"language_code\":\"US\"},\"ewallet_account_"
      "linking_info\":{\"issuer_id\":\"touchngo\"}}");
}

TEST_F(GetDetailsForEwalletAccountLinkingRequestTest,
       VerifyRequestContent_WithClientTokenAndCustomPayload) {
  base::DictValue payload =
      base::DictValue().Set("ewallet_account_linking_info",
                            base::DictValue().Set("issuer_id", "shopeepay"));
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{'a', 'b', 'c'},
      /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true, std::move(payload));

  EXPECT_EQ(request->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/"
            "getdetailsforcreatepaymentinstrument");
  EXPECT_EQ(request->GetRequestContentType(), "application/json");
  EXPECT_EQ(
      request->GetRequestContent(),
      "{\"chrome_user_context\":{\"full_sync_enabled\":true},\"client_"
      "token\":\"YWJj\","
      "\"context\":{\"billable_service\":70073,\"customer_context\":{"
      "\"external_"
      "customer_id\":\"123\"},\"language_code\":\"US\"},\"ewallet_account_"
      "linking_info\":{\"issuer_id\":\"shopeepay\"}}");
}

TEST_F(GetDetailsForEwalletAccountLinkingRequestTest,
       ParseResponse_Success_AccountLinkingEligibilitySetToTrue) {
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true,
      /*account_linking_payload=*/CreateSampleAccountLinkingPayload());
  std::optional<base::Value> response = base::JSONReader::Read(
      R"({"ewallet_account_linking_details":{"action_token":"YWJj"}})",
      base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());

  EXPECT_TRUE(request->IsResponseComplete());
  EXPECT_TRUE(request->is_eligible_for_ewallet_account_linking_);
}

TEST_F(GetDetailsForEwalletAccountLinkingRequestTest,
       ParseResponse_SuccessWithActionToken) {
  std::vector<uint8_t> result_action_token;
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{},
      base::BindOnce(
          [](std::vector<uint8_t>* out_token,
             autofill::payments::PaymentsAutofillClient::PaymentsRpcResult
                 result,
             bool is_eligible, const std::vector<uint8_t>& action_token) {
            *out_token = action_token;
          },
          &result_action_token),
      /*app_locale=*/"US", /*full_sync_enabled=*/true,
      /*account_linking_payload=*/CreateSampleAccountLinkingPayload());

  // "YWJj" is Base64 for "abc" ({'a', 'b', 'c'})
  std::optional<base::Value> response = base::JSONReader::Read(
      R"({"ewallet_account_linking_details":{"action_token":"YWJj"}})",
      base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());
  request->RespondToDelegate(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);

  EXPECT_TRUE(request->is_eligible_for_ewallet_account_linking_);
  EXPECT_EQ(result_action_token, (std::vector<uint8_t>{'a', 'b', 'c'}));
}

TEST_F(
    GetDetailsForEwalletAccountLinkingRequestTest,
    ParseResponse_SuccessWithoutActionToken_AccountLinkingEligibilitySetToFalse) {
  std::vector<uint8_t> result_action_token;
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{},
      base::BindOnce(
          [](std::vector<uint8_t>* out_token,
             autofill::payments::PaymentsAutofillClient::PaymentsRpcResult
                 result,
             bool is_eligible, const std::vector<uint8_t>& action_token) {
            *out_token = action_token;
          },
          &result_action_token),
      /*app_locale=*/"US", /*full_sync_enabled=*/true,
      /*account_linking_payload=*/CreateSampleAccountLinkingPayload());

  std::optional<base::Value> response =
      base::JSONReader::Read(R"({"ewallet_account_linking_details":{}})",
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());
  request->RespondToDelegate(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);

  EXPECT_FALSE(request->is_eligible_for_ewallet_account_linking_);
  EXPECT_TRUE(result_action_token.empty());
}

TEST_F(
    GetDetailsForEwalletAccountLinkingRequestTest,
    ParseResponse_SuccessWithoutEwalletAccountLinkingDetails_AccountLinkingEligibilitySetToFalse) {
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true,
      /*account_linking_payload=*/CreateSampleAccountLinkingPayload());
  std::optional<base::Value> response =
      base::JSONReader::Read("{}", base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());

  EXPECT_TRUE(request->IsResponseComplete());
  EXPECT_FALSE(request->is_eligible_for_ewallet_account_linking_);
}

TEST_F(GetDetailsForEwalletAccountLinkingRequestTest, ParseResponse_Error) {
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true,
      /*account_linking_payload=*/CreateSampleAccountLinkingPayload());
  std::optional<base::Value> response = base::JSONReader::Read(
      "{\"error\":{\"code\":400}}", base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());

  EXPECT_TRUE(request->IsResponseComplete());
  EXPECT_FALSE(request->is_eligible_for_ewallet_account_linking_);
}

TEST_F(GetDetailsForEwalletAccountLinkingRequestTest,
       ParseResponse_ErrorWithDetails) {
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true,
      /*account_linking_payload=*/CreateSampleAccountLinkingPayload());
  std::optional<base::Value> response = base::JSONReader::Read(
      "{\"error\":{\"code\":400},\"ewallet_account_linking_details\":{\"action_"
      "token\":\"YWJj\"}}",
      base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());

  EXPECT_TRUE(request->IsResponseComplete());
  EXPECT_FALSE(request->is_eligible_for_ewallet_account_linking_);
}

TEST_F(GetDetailsForEwalletAccountLinkingRequestTest,
       ParseResponse_CorruptActionToken) {
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true,
      /*account_linking_payload=*/CreateSampleAccountLinkingPayload());
  std::optional<base::Value> response = base::JSONReader::Read(
      "{\"ewallet_account_linking_details\":{\"action_token\":\"not_valid_"
      "base64!!!\"}}",
      base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());

  EXPECT_TRUE(request->IsResponseComplete());
  EXPECT_FALSE(request->is_eligible_for_ewallet_account_linking_);
}

TEST_F(GetDetailsForEwalletAccountLinkingRequestTest,
       ParseResponse_EmptyActionToken) {
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true,
      /*account_linking_payload=*/CreateSampleAccountLinkingPayload());
  std::optional<base::Value> response = base::JSONReader::Read(
      "{\"ewallet_account_linking_details\":{\"action_token\":\"\"}}",
      base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  request->ParseResponse(response->GetDict());

  EXPECT_TRUE(request->IsResponseComplete());
  EXPECT_FALSE(request->is_eligible_for_ewallet_account_linking_);
}

#if GTEST_HAS_DEATH_TEST
TEST_F(GetDetailsForEwalletAccountLinkingRequestTest,
       GetRequestContent_MissingEwalletInfo_CheckFails) {
  auto request = std::make_unique<GetDetailsForEwalletAccountLinkingRequest>(
      123, std::vector<uint8_t>{}, /*response_callback=*/base::DoNothing(),
      /*app_locale=*/"US", /*full_sync_enabled=*/true,
      /*account_linking_payload=*/base::DictValue());

  EXPECT_CHECK_DEATH(request->GetRequestContent());
}
#endif

}  // namespace payments::facilitated
