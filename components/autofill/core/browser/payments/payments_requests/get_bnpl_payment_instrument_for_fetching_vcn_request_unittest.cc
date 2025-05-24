// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_bnpl_payment_instrument_for_fetching_vcn_request.h"

#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::MockCallback;
using Dict = base::Value::Dict;
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
    request_details_.redirect_url = GURL("http://redirect-url.test/");
    request_details_.issuer_id = "ISSUER_ID";

    request_ = std::make_unique<GetBnplPaymentInstrumentForFetchingVcnRequest>(
        request_details_, /*full_sync_enabled=*/true, mock_callback_.Get());
  }

  Dict GetFullResponse() const {
    return Dict().Set(
        "buy_now_pay_later_info",
        Dict().Set(
            "get_vcn_response_info",
            Dict().Set("virtual_card_info",
                       Dict()
                           .Set("pan", "1234")
                           .Set("cvv", "123")
                           .Set("cardholder_name", "Akagi Shigeru")
                           .Set("expiration",
                                Dict().Set("month", 1).Set("year", 2025)))));
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
            "payments/apis-secure/chromepaymentsservice/"
            "getpaymentinstrument?s7e_suffix=chromewallet");
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest,
       GetRequestContentType) {
  EXPECT_EQ(request_->GetRequestContentType(),
            "application/x-www-form-urlencoded");
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest, GetRequestContent) {
  Dict request_dict =
      Dict()
          .Set("context",
               Dict()
                   .Set("billable_service",
                        payments::kUnmaskPaymentMethodBillableServiceNumber)
                   .Set("customer_context",
                        PaymentsRequest::BuildCustomerContextDictionary(
                            request_details_.billing_customer_number)))
          .Set("chrome_user_context", Dict().Set("full_sync_enabled", true))
          .Set("instrument_id", request_details_.instrument_id)
          .Set("risk_data_encoded",
               PaymentsRequest::BuildRiskDictionary(request_details_.risk_data))
          .Set("buy_now_pay_later_info",
               Dict().Set("retrieve_buy_now_pay_later_vcn_request_info",
                          Dict()
                              .Set("get_payment_instrument_context_token",
                                   request_details_.context_token)
                              .Set("redirect_response_url",
                                   request_details_.redirect_url.spec())
                              .Set("issuer_id", request_details_.issuer_id)));

  EXPECT_EQ(request_->GetRequestContent(),
            base::StringPrintf(
                "requestContentType=application/json; charset=utf-8&request=%s",
                base::EscapeUrlEncodedData(
                    base::WriteJson(request_dict).value(), /*use_plus=*/true)
                    .c_str()));
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest,
       IsResponseComplete_ParseResponseNotCalled) {
  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest,
       IsResponseComplete_ParseResponseCalled) {
  request_->ParseResponse(GetFullResponse());

  EXPECT_TRUE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest,
       IsResponseComplete_ParseResponseCalled_EmptyPan) {
  Dict response = GetFullResponse();
  response.RemoveByDottedPath(
      "buy_now_pay_later_info.get_vcn_response_info.virtual_card_info.pan");
  request_->ParseResponse(response);

  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest,
       IsResponseComplete_ParseResponseCalled_EmptyCvv) {
  Dict response = GetFullResponse();
  response.RemoveByDottedPath(
      "buy_now_pay_later_info.get_vcn_response_info.virtual_card_info.cvv");
  request_->ParseResponse(response);

  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest,
       IsResponseComplete_ParseResponseCalled_EmptyCardholderName) {
  Dict response = GetFullResponse();
  response.RemoveByDottedPath(
      "buy_now_pay_later_info.get_vcn_response_info.virtual_card_info."
      "cardholder_name");
  request_->ParseResponse(response);

  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest,
       IsResponseComplete_ParseResponseCalled_EmptyExpirationMonth) {
  Dict response = GetFullResponse();
  response.RemoveByDottedPath(
      "buy_now_pay_later_info.get_vcn_response_info.virtual_card_info."
      "expiration.month");
  request_->ParseResponse(response);

  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest,
       IsResponseComplete_ParseResponseCalled_EmptyExpirationYear) {
  Dict response = GetFullResponse();
  response.RemoveByDottedPath(
      "buy_now_pay_later_info.get_vcn_response_info.virtual_card_info."
      "expiration.year");
  request_->ParseResponse(response);

  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingVcnRequestTest, RespondToDelegate) {
  request_->ParseResponse(GetFullResponse());

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
