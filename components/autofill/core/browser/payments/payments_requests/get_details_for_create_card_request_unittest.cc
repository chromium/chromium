// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_details_for_create_card_request.h"

#include "base/functional/callback_helpers.h"
#include "base/test/values_test_util.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_create_card_request_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

constexpr char kUniqueCountryCode[] = "US";
constexpr char kAppLocale[] = "en";
constexpr int64_t kBillingCustomerNumber = 111122223333;

std::unique_ptr<GetDetailsForCreateCardRequest> CreateRequest() {
  std::vector<ClientBehaviorConstants> client_behavior_signals{
      ClientBehaviorConstants::kOfferingToSaveCvc};
  return std::make_unique<GetDetailsForCreateCardRequest>(
      kUniqueCountryCode, client_behavior_signals, kAppLocale,
      /*callback=*/base::DoNothing(), kUploadPaymentMethodBillableServiceNumber,
      kBillingCustomerNumber, UploadCardSource::kUpstreamSaveAndFill);
}

TEST(GetDetailsForCreateCardRequestTest,
     GetRequestContent_ContainsExpectedData) {
  std::unique_ptr<GetDetailsForCreateCardRequest> request = CreateRequest();
  base::Value::Dict expected_request_content =
      base::Value::Dict()
          .Set("context",
               base::Value::Dict()
                   .Set("billable_service",
                        payments::kUploadPaymentMethodBillableServiceNumber)
                   .Set("customer_context",
                        PaymentsRequest::BuildCustomerContextDictionary(
                            kBillingCustomerNumber))
                   .Set("language_code", "en"))
          .Set("chrome_user_context",
               base::Value::Dict().Set(
                   "client_behavior_signals",
                   base::Value::List().Append(static_cast<int>(
                       ClientBehaviorConstants::kOfferingToSaveCvc))))
          .Set("card_info",
               base::Value::Dict()
                   .Set("unique_country_code", "US")
                   .Set("upload_card_source", "UPSTREAM_SAVE_AND_FILL"));

  EXPECT_THAT(request->GetRequestContent(),
              base::test::IsJson(expected_request_content));
}

TEST(GetDetailsForCreateCardRequestTest, ParseResponse_ResponseIsComplete) {
  std::unique_ptr<GetDetailsForCreateCardRequest> request = CreateRequest();
  base::Value::Dict response =
      base::Value::Dict()
          .Set("context_token", u"some token")
          .Set("legal_message",
               base::Value::Dict().Set("terms_of_service", "Terms of Service"))
          .Set("card_details",
               base::Value::Dict().Set("supported_card_bin_ranges_string",
                                       "1234,30000-55555,765"));

  request->ParseResponse(response);

  EXPECT_EQ(test_api(*request).context_token(), u"some token");
  EXPECT_TRUE(test_api(*request).legal_message());
  EXPECT_EQ(test_api(*request).supported_card_bin_ranges().size(), 3UL);
  EXPECT_TRUE(request->IsResponseComplete());
}

TEST(GetDetailsForCreateCardRequestTest, ParseResponse_MissingContextToken) {
  std::unique_ptr<GetDetailsForCreateCardRequest> request = CreateRequest();
  base::Value::Dict response =
      base::Value::Dict()
          .Set("legal_message",
               base::Value::Dict().Set("terms_of_service", "Terms of Service"))
          .Set("card_details",
               base::Value::Dict().Set("supported_card_bin_ranges_string",
                                       "1234,30000-55555,765"));

  request->ParseResponse(response);

  EXPECT_FALSE(request->IsResponseComplete());
}

TEST(GetDetailsForCreateCardRequestTest, ParseResponse_MissingLegalMessage) {
  std::unique_ptr<GetDetailsForCreateCardRequest> request = CreateRequest();
  base::Value::Dict response =
      base::Value::Dict()
          .Set("context_token", u"some token")
          .Set("card_details",
               base::Value::Dict().Set("supported_card_bin_ranges_string",
                                       "1234,30000-55555,765"));

  request->ParseResponse(response);

  EXPECT_FALSE(request->IsResponseComplete());
}

TEST(GetDetailsForCreateCardRequestTest,
     ParseResponse_MissingSupportedCardBinRanges) {
  std::unique_ptr<GetDetailsForCreateCardRequest> request = CreateRequest();
  base::Value::Dict response =
      base::Value::Dict()
          .Set("context_token", u"some token")
          .Set("legal_message",
               base::Value::Dict().Set("terms_of_service", "Terms of Service"));

  request->ParseResponse(response);

  EXPECT_TRUE(request->IsResponseComplete());
}

}  // namespace

}  // namespace autofill::payments
