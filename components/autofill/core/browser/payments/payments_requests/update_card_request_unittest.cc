// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/update_card_request.h"

#include <memory>
#include <string>

#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

constexpr int64_t kTestBillingCustomerNumber = 1234567890;
constexpr int64_t kTestInstrumentId = 987654321;
constexpr char kTestAppLocale[] = "en-US";
constexpr char kTestContextToken[] = "test_context_token_12345";
constexpr char kTestRiskData[] = "test_risk_data";
constexpr char16_t kTestCardholderName[] = u"John Doe";
constexpr char16_t kTestCvc[] = u"123";

class UpdateCardRequestTest : public testing::Test {
 public:
  void SetUp() override {
    request_details_.app_locale = kTestAppLocale;
    request_details_.billing_customer_number = kTestBillingCustomerNumber;
    request_details_.instrument_id = kTestInstrumentId;
    request_details_.cardholder_name = kTestCardholderName;
    request_details_.cvc = kTestCvc;
    request_details_.context_token = kTestContextToken;
    request_details_.risk_data = kTestRiskData;
  }

  std::unique_ptr<UpdateCardRequest> CreateRequest() {
    return std::make_unique<UpdateCardRequest>(request_details_,
                                               mock_callback_.Get());
  }

 protected:
  UpdateCardRequestDetails request_details_;
  base::MockOnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
      mock_callback_;
};

TEST_F(UpdateCardRequestTest, GetRequestUrlPath) {
  std::unique_ptr<UpdateCardRequest> request = CreateRequest();
  EXPECT_EQ(request->GetRequestUrlPath(),
            "payments/apis-secure/chromepaymentsservice/"
            "updatepaymentinstrument?s7e_suffix=chromewallet");
}

TEST_F(UpdateCardRequestTest, GetRequestContentType) {
  std::unique_ptr<UpdateCardRequest> request = CreateRequest();
  EXPECT_EQ(request->GetRequestContentType(),
            "application/x-www-form-urlencoded");
}

TEST_F(UpdateCardRequestTest, GetRequestContent) {
  std::unique_ptr<UpdateCardRequest> request = CreateRequest();

  std::string content = request->GetRequestContent();

  // Content should contain form fields: requestContentType, request, and
  // s7e_13_cvc.
  EXPECT_NE(content.find("requestContentType=application/json; charset=utf-8"),
            std::string::npos);
  EXPECT_NE(content.find("&s7e_13_cvc=" + base::UTF16ToASCII(kTestCvc)),
            std::string::npos);

  // Extract the URL-encoded inner JSON request parameter.
  const std::string prefix = "&request=";
  size_t start = content.find(prefix);
  ASSERT_NE(start, std::string::npos);
  start += prefix.length();
  size_t end = content.find("&s7e_13_cvc=", start);
  ASSERT_NE(end, std::string::npos);

  std::string unescaped_json = base::UnescapeURLComponent(
      content.substr(start, end - start),
      base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
          base::UnescapeRule::REPLACE_PLUS_WITH_SPACE);

  base::DictValue request_dict = base::test::ParseJsonDict(unescaped_json);

  // Verify context.
  const base::DictValue* context = request_dict.FindDict("context");
  ASSERT_TRUE(context);
  EXPECT_EQ(*context->FindString("language_code"), kTestAppLocale);
  EXPECT_EQ(context->FindInt("billable_service"),
            kUploadPaymentMethodBillableServiceNumber);
  const base::DictValue* customer_context =
      context->FindDict("customer_context");
  ASSERT_TRUE(customer_context);
  EXPECT_EQ(*customer_context->FindString("external_customer_id"),
            base::NumberToString(kTestBillingCustomerNumber));

  // Verify chrome_user_context is present.
  const base::DictValue* chrome_user_context =
      request_dict.FindDict("chrome_user_context");
  ASSERT_TRUE(chrome_user_context);

  // Verify instrument_id.
  EXPECT_EQ(*request_dict.FindString("instrument_id"),
            base::NumberToString(kTestInstrumentId));

  // Verify context_token.
  EXPECT_EQ(*request_dict.FindString("context_token"), kTestContextToken);

  // Verify risk data.
  const base::DictValue* risk_dict = request_dict.FindDict("risk_data_encoded");
  ASSERT_TRUE(risk_dict);
  EXPECT_EQ(*risk_dict->FindString("value"), kTestRiskData);

  // Verify card_info.
  const base::DictValue* card_info = request_dict.FindDict("card_info");
  ASSERT_TRUE(card_info);
  EXPECT_EQ(*card_info->FindString("cardholder_name"),
            base::UTF16ToUTF8(kTestCardholderName));
  EXPECT_EQ(*card_info->FindString("encrypted_cvc"), "__param:s7e_13_cvc");
}

TEST_F(UpdateCardRequestTest, ParseResponse_SuccessWithCardInfo) {
  std::unique_ptr<UpdateCardRequest> request = CreateRequest();

  base::DictValue response;
  response.Set("card_info", base::DictValue());

  request->ParseResponse(response);

  EXPECT_TRUE(request->IsResponseComplete());
}

TEST_F(UpdateCardRequestTest, ParseResponse_MissingCardInfo_Incomplete) {
  std::unique_ptr<UpdateCardRequest> request = CreateRequest();

  base::DictValue response;
  request->ParseResponse(response);

  EXPECT_FALSE(request->IsResponseComplete());
}

TEST_F(UpdateCardRequestTest, RespondToDelegate) {
  std::unique_ptr<UpdateCardRequest> request = CreateRequest();

  base::DictValue response;
  response.Set("card_info", base::DictValue());
  request->ParseResponse(response);

  EXPECT_CALL(mock_callback_,
              Run(PaymentsAutofillClient::PaymentsRpcResult::kSuccess));

  request->RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess);
}

}  // namespace
}  // namespace autofill::payments
