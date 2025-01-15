// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_bnpl_payment_instrument_for_fetching_vcn_request.h"

#include "base/json/json_writer.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::MockCallback;
using base::WriteJson;
using base::test::IsJson;
using std::string;
using testing::Field;
}  // namespace

namespace autofill::payments {

class GetBnplPaymentInstrumentForFetchingVcnRequestTest : public testing::Test {
 public:
  void SetUp() override {
    request_details_.billing_customer_number = 1234;
    request_details_.risk_data = "RISK_DATA";
    request_details_.instrument_id = "INSTRUMENT_ID";
    request_details_.context_token = "CONTEXT_TOKEN";
    request_details_.redirect_url = GURL("http://redirect.url/");

    request_ = std::make_unique<GetBnplPaymentInstrumentForFetchingVcnRequest>(
        request_details_, /*full_sync_enabled=*/true, mock_callback_.Get());
  }

  GetBnplPaymentInstrumentForFetchingVcnRequestDetails request_details_;
  MockCallback<
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const BnplFetchVcnResponseDetails&)>>
      mock_callback_;

  std::unique_ptr<GetBnplPaymentInstrumentForFetchingVcnRequest> request_;
};

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest, GetRequestUrlPath) {
  EXPECT_EQ(request_->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/getpaymentinstrument");
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest,
       GetRequestContentType) {
  EXPECT_EQ(request_->GetRequestContentType(), "application/json");
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest, GetRequestContent) {
  base::Value::Dict request_dict =
      base::Value::Dict()
          .Set("chrome_user_context",
               base::Value::Dict().Set("full_sync_enabled", true))
          .Set("context",
               base::Value::Dict()
                   .Set("billable_service",
                        payments::kUnmaskPaymentMethodBillableServiceNumber)
                   .Set("customer_context",
                        PaymentsRequest::BuildCustomerContextDictionary(
                            request_details_.billing_customer_number)))
          .Set("risk_data_encoded",
               PaymentsRequest::BuildRiskDictionary(request_details_.risk_data))
          .Set("instrument_id", request_details_.instrument_id)
          .Set("buy_now_pay_later_info",
               base::Value::Dict().Set(
                   "retrieve_buy_now_pay_later_vcn_request_info",
                   base::Value::Dict()
                       .Set("get_payment_instrument_context_token",
                            request_details_.context_token)
                       .Set("redirect_response_url",
                            request_details_.redirect_url.spec())));

  EXPECT_THAT(request_->GetRequestContent(), IsJson(request_dict));
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest,
       IsResponseComplete_ParseResponseNotCalled) {
  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest,
       IsResponseComplete_ParseResponseCalled) {
  request_->ParseResponse(base::Value::Dict().SetByDottedPath(
      "buy_now_pay_later_info.get_vcn_response_info.virtual_card_info.pan",
      "1234"));

  EXPECT_TRUE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest, RespondToDelegate) {
  base::Value::Dict response_dict = base::Value::Dict().Set(
      "buy_now_pay_later_info",
      base::Value::Dict().Set(
          "get_vcn_response_info",
          base::Value::Dict().Set(
              "virtual_card_info",
              base::Value::Dict()
                  .Set("pan", "1234")
                  .Set("cvv", "123")
                  .Set("cardholder_name", "Akagi Shigeru")
                  .Set(
                      "expiration",
                      base::Value::Dict().Set("month", 1).Set("year", 2025)))));

  request_->ParseResponse(response_dict);

  EXPECT_CALL(
      mock_callback_,
      Run(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
          /*response_details=*/AllOf(
              Field(&BnplFetchVcnResponseDetails::pan, "1234"),
              Field(&BnplFetchVcnResponseDetails::cvv, "123"),
              Field(&BnplFetchVcnResponseDetails::cardholder_name,
                    "Akagi Shigeru"),
              Field(&BnplFetchVcnResponseDetails::expiration_month, "1"),
              Field(&BnplFetchVcnResponseDetails::expiration_year, "2025"))));

  request_->RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess);
}

}  // namespace autofill::payments
