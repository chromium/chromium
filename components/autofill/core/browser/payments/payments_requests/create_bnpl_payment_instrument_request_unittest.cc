// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/create_bnpl_payment_instrument_request.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/payments/payments_requests/create_bnpl_payment_instrument_request_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

constexpr char kAppLocale[] = "dummy_locale";
constexpr int64_t kBillingCustomerNumber = 111222333;
constexpr char kContextToken[] = "somecontexttoken";
constexpr char kEncodedRiskData[] = "wjhJLga67gowLp3vIbJ4W";

class CreateBnplPaymentInstrumentRequestTest : public testing::Test {
 public:
  void SetUp() override {
    CreateBnplPaymentInstrumentRequestDetails request_details;
    request_details.app_locale = kAppLocale;
    request_details.billing_customer_number = kBillingCustomerNumber;
    request_details.context_token = kContextToken;
    request_details.risk_data = kEncodedRiskData;
    request_ = std::make_unique<CreateBnplPaymentInstrumentRequest>(
        request_details, /*full_sync_enabled=*/true, base::DoNothing());
  }

  CreateBnplPaymentInstrumentRequest* GetRequest() { return request_.get(); }

  void ParseResponse(const base::Value::Dict& response) {
    request_->ParseResponse(response);
  }

  bool IsResponseComplete() const { return request_->IsResponseComplete(); }

 private:
  std::unique_ptr<CreateBnplPaymentInstrumentRequest> request_;
};

TEST_F(CreateBnplPaymentInstrumentRequestTest,
       GetRequestContent_ContainsExpectedData) {
  EXPECT_EQ(GetRequest()->GetRequestUrlPath(),
            "payments/apis-secure/chromepaymentsservice/"
            "createpaymentinstrument");
  ASSERT_FALSE(GetRequest()->GetRequestContent().empty());
  EXPECT_NE(GetRequest()->GetRequestContent().find("language_code"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find(kAppLocale),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("billable_service"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find(base::NumberToString(
                kUploadPaymentMethodBillableServiceNumber)),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("customer_context"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("external_customer_id"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find(
                base::NumberToString(kBillingCustomerNumber)),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("context_token"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find(kContextToken),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("chrome_user_context"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("risk_data_encoded"),
            std::string::npos);
}

TEST_F(CreateBnplPaymentInstrumentRequestTest,
       ParseResponse_ResponseIsComplete) {
  base::Value::Dict response = base::Value::Dict().Set(
      "buy_now_pay_later_info",
      base::Value::Dict().Set("instrument_id",
                              base::Value("some instrument id")));

  ParseResponse(response);

  EXPECT_EQ(test_api(*GetRequest()).get_instrument_id(), "some instrument id");
  EXPECT_TRUE(IsResponseComplete());
}

TEST_F(CreateBnplPaymentInstrumentRequestTest,
       ParseResponse_MissingInstrumentId) {
  base::Value::Dict response = base::Value::Dict();

  ParseResponse(response);

  EXPECT_FALSE(IsResponseComplete());
}

}  // namespace
}  // namespace autofill::payments
