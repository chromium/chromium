// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/create_card_request.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request_constants.h"
#include "components/autofill/core/browser/payments/test/autofill_payments_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

namespace {

struct CreateCardOptions {
  CreateCardOptions& with_cvc_in_request(bool b) {
    include_cvc = b;
    return *this;
  }

  CreateCardOptions& with_nickname_in_request(bool b) {
    include_nickname = b;
    return *this;
  }

  CreateCardOptions& with_address_in_request(bool b) {
    include_address = b;
    return *this;
  }

  bool include_cvc = true;
  bool include_nickname = true;
  bool include_address = true;
};

std::unique_ptr<CreateCardRequest> BuildCreateCardRequest(
    const CreateCardOptions& option = CreateCardOptions()) {
  UploadCardRequestDetails request_details;
  request_details.billing_customer_number = 111122223333;
  request_details.card = test::GetCreditCard();
  if (option.include_cvc) {
    request_details.cvc = u"123";
  }
  if (option.include_nickname) {
    request_details.card.SetNickname(u"some nickname");
  }
  request_details.client_behavior_signals = {
      ClientBehaviorConstants::kOfferingToSaveCvc};
  request_details.context_token = u"some context token";
  request_details.risk_data = "some risk data";
  request_details.app_locale = "en";
  if (option.include_address) {
    request_details.profiles.emplace_back(
        test::GetFullProfile(AddressCountryCode("US")));
  }
  request_details.upload_card_source = UploadCardSource::kUpstreamSaveAndFill;

  return std::make_unique<CreateCardRequest>(request_details,
                                             base::DoNothing());
}

TEST(CreateCardRequestTest, GetRequestUrlPath) {
  std::unique_ptr<CreateCardRequest> request = BuildCreateCardRequest();

  EXPECT_EQ(request->GetRequestUrlPath(),
            "payments/apis-secure/chromepaymentsservice/"
            "createpaymentinstrument?s7e_suffix=chromewallet");
}

TEST(CreateCardRequestTest, GetRequestContent_ContainsExpectedData) {
  std::unique_ptr<CreateCardRequest> request = BuildCreateCardRequest();
  base::Value::Dict address =
      base::Value::Dict()
          .Set("phone_number", "16502111111")
          .Set("postal_address",
               base::Value::Dict()
                   .Set("address_line", base::Value::List()
                                            .Append("666 Erebus St.")
                                            .Append("Apt 8"))
                   .Set("administrative_area_name", "CA")
                   .Set("country_name_code", "US")
                   .Set("locality_name", "Elysium")
                   .Set("postal_code_number", "91111")
                   .Set("recipient_name", "John H. Doe"));
  int exp_month, exp_year;
  base::StringToInt(test::NextMonth(), &exp_month);
  base::StringToInt(test::NextYear(), &exp_year);
  base::Value::Dict json_dict =
      base::Value::Dict()
          .Set("context",
               base::Value::Dict()
                   .Set("billable_service",
                        payments::kUploadPaymentMethodBillableServiceNumber)
                   .Set("customer_context",
                        PaymentsRequest::BuildCustomerContextDictionary(
                            111122223333))
                   .Set("language_code", "en"))
          .Set("chrome_user_context",
               base::Value::Dict().Set(
                   "client_behavior_signals",
                   base::Value::List().Append(static_cast<int>(
                       ClientBehaviorConstants::kOfferingToSaveCvc))))
          .Set("context_token", "some context token")
          .Set("risk_data_encoded",
               PaymentsRequest::BuildRiskDictionary("some risk data"))
          .Set("nickname", "some nickname")
          .Set("card_info",
               base::Value::Dict()
                   .Set("pan", "__param:s7e_21_pan")
                   .Set("cvc", "__param:s7e_13_cvc")
                   .Set("expiration_month", exp_month)
                   .Set("expiration_year", exp_year)
                   .Set("cardholder_name", "Test User")
                   .Set("address", std::move(address))
                   .Set("upload_card_source", "UPSTREAM_SAVE_AND_FILL"));
  std::string expected_json_content = base::WriteJson(json_dict).value_or("");

  std::string expected_request_content = base::StringPrintf(
      "requestContentType=application/json; charset=utf-8&request=%s"
      "&s7e_21_pan=%s&s7e_13_cvc=%s",
      base::EscapeUrlEncodedData(expected_json_content, true).c_str(),
      base::EscapeUrlEncodedData(base::UTF16ToASCII(u"4111111111111111"), true)
          .c_str(),
      base::EscapeUrlEncodedData(base::UTF16ToASCII(u"123"), true).c_str());

  EXPECT_EQ(request->GetRequestContent(), expected_request_content);
}

TEST(CreateCardRequestTest, NoCvcInRequestIfNotProvided) {
  std::unique_ptr<CreateCardRequest> request =
      BuildCreateCardRequest(CreateCardOptions().with_cvc_in_request(false));

  EXPECT_TRUE(!request->GetRequestContent().empty());
  // Verify that the encrypted_cvc and s7e_13_cvc parameters were not included
  // in the request.
  EXPECT_TRUE(request->GetRequestContent().find("encrypted_cvc") ==
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("__param:s7e_13_cvc") ==
              std::string::npos);
  EXPECT_TRUE(request->GetRequestContent().find("&s7e_13_cvc=") ==
              std::string::npos);
}

TEST(CreateCardRequestTest, NoNicknameInRequestIfNicknameNotProvided) {
  std::unique_ptr<CreateCardRequest> request = BuildCreateCardRequest(
      CreateCardOptions().with_nickname_in_request(false));

  // Card nickname was not set.
  EXPECT_TRUE(request->GetRequestContent().find("nickname") ==
              std::string::npos);
}

TEST(CreateCardRequestTest, NoAddressInRequestIfAddressNotProvided) {
  std::unique_ptr<CreateCardRequest> request = BuildCreateCardRequest(
      CreateCardOptions().with_address_in_request(false));

  // Address was not set.
  EXPECT_TRUE(request->GetRequestContent().find("address") ==
              std::string::npos);
}

TEST(CreateCardRequestTest, GetTimeout) {
  std::unique_ptr<CreateCardRequest> request = BuildCreateCardRequest();
  EXPECT_EQ(request->GetTimeout(), kUploadCardRequestTimeout);
}

TEST(CreateCardRequestTest, ParseResponse) {
  std::unique_ptr<CreateCardRequest> request = BuildCreateCardRequest();

  base::Value::Dict response = base::Value::Dict().Set(
      "card_info", base::Value::Dict().Set("instrument_id", "11223344"));

  request->ParseResponse(response);

  EXPECT_EQ(request->GetInstrumentIdForTesting(), "11223344");
  EXPECT_TRUE(request->IsResponseComplete());
}

TEST(CreateCardRequestTest, ParseResponse_MissingCardInfo) {
  std::unique_ptr<CreateCardRequest> request = BuildCreateCardRequest();

  request->ParseResponse(base::Value::Dict());

  EXPECT_FALSE(request->IsResponseComplete());
}

}  // namespace

}  // namespace autofill::payments
