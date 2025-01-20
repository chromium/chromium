// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_details_for_create_bnpl_payment_instrument_request.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_create_bnpl_payment_instrument_request_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

constexpr char kAppLocale[] = "dummy_locale";
constexpr char kIssuerId[] = "affirm";
constexpr int64_t kBillingCustomerNumber = 111222333;

class GetDetailsForCreateBnplPaymentInstrumentRequestTest
    : public testing::Test {
 public:
  void SetUp() override {
    GetDetailsForCreateBnplPaymentInstrumentRequestDetails request_details;
    request_details.app_locale = kAppLocale;
    request_details.issuer_id = kIssuerId;
    request_details.billing_customer_number = kBillingCustomerNumber;
    request_ =
        std::make_unique<GetDetailsForCreateBnplPaymentInstrumentRequest>(
            request_details, /*full_sync_enabled=*/true, base::DoNothing());
  }

  GetDetailsForCreateBnplPaymentInstrumentRequest* GetRequest() {
    return request_.get();
  }

  void ParseResponse(const base::Value::Dict& response) {
    request_->ParseResponse(response);
  }

  bool IsResponseComplete() const { return request_->IsResponseComplete(); }

 private:
  std::unique_ptr<GetDetailsForCreateBnplPaymentInstrumentRequest> request_;
};

TEST_F(GetDetailsForCreateBnplPaymentInstrumentRequestTest,
       GetRequestContent_ContainsExpectedData) {
  EXPECT_EQ(GetRequest()->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/"
            "getdetailsforcreatepaymentinstrument");
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
  EXPECT_NE(GetRequest()->GetRequestContent().find("chrome_user_context"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("issuer_id"),
            std::string::npos);
}

TEST_F(GetDetailsForCreateBnplPaymentInstrumentRequestTest,
       ParseResponse_ResponseIsComplete) {
  base::Value::Dict response =
      base::Value::Dict()
          .Set("context_token", base::Value(u"some token"))
          .Set("legal_message",
               base::Value::Dict().Set("terms_of_service", "Terms of Service"));

  ParseResponse(response);

  EXPECT_EQ(test_api(*GetRequest()).get_context_token(), u"some token");
  EXPECT_TRUE(test_api(*GetRequest()).get_legal_message());
  EXPECT_TRUE(IsResponseComplete());
}

TEST_F(GetDetailsForCreateBnplPaymentInstrumentRequestTest,
       ParseResponse_MissingContextToken) {
  base::Value::Dict response = base::Value::Dict().Set(
      "legal_message",
      base::Value::Dict().Set("terms_of_service", "Terms of Service"));

  ParseResponse(response);

  EXPECT_FALSE(IsResponseComplete());
}

TEST_F(GetDetailsForCreateBnplPaymentInstrumentRequestTest,
       ParseResponse_MissingLegalMessage) {
  base::Value::Dict response =
      base::Value::Dict().Set("context_token", base::Value(u"some token"));

  ParseResponse(response);

  EXPECT_FALSE(IsResponseComplete());
}

}  // namespace
}  // namespace autofill::payments
