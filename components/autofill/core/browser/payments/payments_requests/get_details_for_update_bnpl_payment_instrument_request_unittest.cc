// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_details_for_update_bnpl_payment_instrument_request.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_update_bnpl_payment_instrument_request_test_api.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

constexpr char kAppLocale[] = "dummy_locale";
constexpr int64_t kBillingCustomerNumber = 111222333;
constexpr int64_t kInstrumentId = 555666777;

MATCHER_P(HasLegalMessageLineText, text, "A LegalMessageLine that has text.") {
  return base::UTF16ToUTF8(arg.text()) == text;
}

class GetDetailsForUpdateBnplPaymentInstrumentRequestTest
    : public testing::Test {
 public:
  void SetUp() override {
    GetDetailsForUpdateBnplPaymentInstrumentRequestDetails request_details;
    request_details.app_locale = kAppLocale;
    request_details
        .type = GetDetailsForUpdateBnplPaymentInstrumentRequestDetails::
        GetDetailsForUpdateBnplPaymentInstrumentType::kGetDetailsForAcceptTos;
    request_details.billing_customer_number = kBillingCustomerNumber;
    request_details.client_behavior_signals = {
        ClientBehaviorConstants::kShowAccountEmailInLegalMessage};
    request_details.instrument_id = kInstrumentId;
    request_details.issuer_id = kBnplKlarnaIssuerId;
    request_ =
        std::make_unique<GetDetailsForUpdateBnplPaymentInstrumentRequest>(
            request_details, /*full_sync_enabled=*/true, base::DoNothing());
  }

  GetDetailsForUpdateBnplPaymentInstrumentRequest* GetRequest() {
    return request_.get();
  }

  void ParseResponse(const base::Value::Dict& response) {
    request_->ParseResponse(response);
  }

  bool IsResponseComplete() const { return request_->IsResponseComplete(); }

 private:
  std::unique_ptr<GetDetailsForUpdateBnplPaymentInstrumentRequest> request_;
};

TEST_F(GetDetailsForUpdateBnplPaymentInstrumentRequestTest,
       GetRequestContent_ContainsExpectedData) {
  EXPECT_EQ(GetRequest()->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/"
            "getdetailsforupdatepaymentinstrument");
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
  // Verify client_behavior_signal was set.
  // ClientBehaviorConstants::kShowAccountEmailInLegalMessage has the numeric
  // value set to 4.
  EXPECT_NE(
      GetRequest()->GetRequestContent().find("\"client_behavior_signals\":[4]"),
      std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("buy_now_pay_later_info"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("type"), std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("issuer_id"),
            std::string::npos);
  EXPECT_NE(GetRequest()->GetRequestContent().find("instrument_id"),
            std::string::npos);
}

TEST_F(GetDetailsForUpdateBnplPaymentInstrumentRequestTest,
       ParseResponse_ResponseIsComplete) {
  base::Value::Dict response =
      base::Value::Dict()
          .Set("context_token", base::Value(u"some token"))
          .Set("buy_now_pay_later_details",
               base::Value::Dict().Set(
                   "legal_message",
                   base::Value::Dict().Set(
                       "line",
                       base::Value::List().Append(base::Value::Dict().Set(
                           "template", "Terms of Service")))));

  ParseResponse(response);

  EXPECT_EQ(test_api(*GetRequest()).get_context_token(), "some token");
  EXPECT_THAT(
      test_api(*GetRequest()).get_legal_message(),
      testing::ElementsAre(HasLegalMessageLineText("Terms of Service")));
  EXPECT_TRUE(IsResponseComplete());
}

TEST_F(GetDetailsForUpdateBnplPaymentInstrumentRequestTest,
       ParseResponse_MissingContextToken) {
  base::Value::Dict response = base::Value::Dict().Set(
      "buy_now_pay_later_details",
      base::Value::Dict().Set(
          "legal_message",
          base::Value::Dict().Set(
              "line", base::Value::List().Append(base::Value::Dict().Set(
                          "template", "Terms of Service")))));

  ParseResponse(response);

  EXPECT_FALSE(IsResponseComplete());
}

TEST_F(GetDetailsForUpdateBnplPaymentInstrumentRequestTest,
       ParseResponse_InvalidLegalMessage) {
  base::Value::Dict response =
      base::Value::Dict()
          .Set("context_token", base::Value(u"some token"))
          .Set("buy_now_pay_later_details",
               base::Value::Dict().Set(
                   "legal_message",
                   base::Value::Dict().Set(
                       "dummy",
                       base::Value::List().Append(base::Value::Dict().Set(
                           "template", "Terms of Service")))));

  ParseResponse(response);

  EXPECT_FALSE(IsResponseComplete());
}

TEST_F(GetDetailsForUpdateBnplPaymentInstrumentRequestTest,
       ParseResponse_MissingLegalMessage) {
  base::Value::Dict response =
      base::Value::Dict().Set("context_token", base::Value(u"some token"));

  ParseResponse(response);

  EXPECT_FALSE(IsResponseComplete());
}

}  // namespace
}  // namespace autofill::payments
