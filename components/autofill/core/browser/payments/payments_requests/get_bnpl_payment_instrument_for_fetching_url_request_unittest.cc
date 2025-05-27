// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_bnpl_payment_instrument_for_fetching_url_request.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::MockCallback;
using base::test::IsJson;
using testing::Field;
using Dict = base::Value::Dict;
using base::OnceCallback;
using PaymentsRpcResult =
    autofill::payments::PaymentsAutofillClient::PaymentsRpcResult;
}  // namespace

namespace autofill::payments {

class GetBnplPaymentInstrumentForFetchingUrlRequestTest : public testing::Test {
 public:
  void SetUp() override {
    request_details_.billing_customer_number = 1234;
    request_details_.instrument_id = "INSTRUMENT_ID";
    request_details_.risk_data = "RISK_DATA";
    request_details_.merchant_domain = GURL("http://merchant-domain.test/");
    request_details_.total_amount = 1000000000;
    request_details_.currency = "CAD";

    request_ = std::make_unique<GetBnplPaymentInstrumentForFetchingUrlRequest>(
        request_details_, /*full_sync_enabled=*/true, mock_callback_.Get());
  }

  Dict GetFullResponse() {
    return Dict()
        .SetByDottedPath(
            "buy_now_pay_later_info.get_redirect_url_response_"
            "info.redirect_url",
            "http://redirect-url.test/")
        .SetByDottedPath(
            "buy_now_pay_later_info.get_redirect_url_response_"
            "info.base_success_return_url",
            "http://success-url.test/")
        .SetByDottedPath(
            "buy_now_pay_later_info.get_redirect_url_response_"
            "info.base_failure_return_url",
            "http://failure-url.test/")
        .SetByDottedPath(
            "buy_now_pay_later_info.get_redirect_url_response_"
            "info.get_payment_instrument_context_token",
            "sometestcontexttoken");
  }

  GetBnplPaymentInstrumentForFetchingUrlRequestDetails request_details_;
  MockCallback<
      OnceCallback<void(PaymentsRpcResult, const BnplFetchUrlResponseDetails&)>>
      mock_callback_;

  std::unique_ptr<GetBnplPaymentInstrumentForFetchingUrlRequest> request_;
};

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest, GetRequestUrlPath) {
  EXPECT_EQ(request_->GetRequestUrlPath(),
            "payments/apis-secure/chromepaymentsservice/getpaymentinstrument");
}

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest,
       GetRequestContentType) {
  EXPECT_EQ(request_->GetRequestContentType(), "application/json");
}

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest, GetRequestContent) {
  Dict expected_request_dict =
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
               Dict().Set(
                   "initiate_buy_now_pay_later_request_info",
                   Dict()
                       .Set("merchant_domain",
                            request_details_.merchant_domain.spec())
                       .Set("cart_total_amount",
                            Dict()
                                .Set("amount_in_micros",
                                     base::NumberToString(
                                         request_details_.total_amount))
                                .Set("currency", request_details_.currency))));

  EXPECT_THAT(request_->GetRequestContent(), IsJson(expected_request_dict));
}

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest,
       IsResponseComplete_ParseResponseNotCalled) {
  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest,
       IsResponseComplete_ParseResponseCalled) {
  request_->ParseResponse(GetFullResponse());

  EXPECT_TRUE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest,
       IsResponseComplete_ParseResponseCalled_NoRedirectUrl) {
  Dict response = GetFullResponse();
  response.RemoveByDottedPath(
      "buy_now_pay_later_info.get_redirect_url_response_"
      "info.redirect_url");
  request_->ParseResponse(response);
  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest,
       IsResponseComplete_ParseResponseCalled_InvalidRedirectUrl) {
  Dict response = GetFullResponse();
  response.RemoveByDottedPath(
      "buy_now_pay_later_info.get_redirect_url_response_"
      "info.redirect_url");
  response.SetByDottedPath(
      "buy_now_pay_later_info.get_redirect_url_response_"
      "info.redirect_url",
      "invalidredirecturl!!$$");
  request_->ParseResponse(response);
  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest,
       IsResponseComplete_ParseResponseCalled_NoSuccessUrlPrefix) {
  Dict response = GetFullResponse();
  response.RemoveByDottedPath(
      "buy_now_pay_later_info.get_redirect_url_response_"
      "info.base_success_return_url");
  request_->ParseResponse(response);

  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest,
       IsResponseComplete_ParseResponseCalled_InvalidSuccessUrlPrefix) {
  Dict response = GetFullResponse();
  response.RemoveByDottedPath(
      "buy_now_pay_later_info.get_redirect_url_response_"
      "info.base_success_return_url");
  response.SetByDottedPath(
      "buy_now_pay_later_info.get_redirect_url_response_"
      "info.base_success_return_url",
      "invalidsuccessurl!!$$");
  request_->ParseResponse(response);

  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest,
       IsResponseComplete_ParseResponseCalled_NoFailureUrlPrefix) {
  Dict response = GetFullResponse();
  response.RemoveByDottedPath(
      "buy_now_pay_later_info.get_redirect_url_response_"
      "info.base_failure_return_url");
  request_->ParseResponse(response);

  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest,
       IsResponseComplete_ParseResponseCalled_InvalidFailureUrlPrefix) {
  Dict response = GetFullResponse();
  response.RemoveByDottedPath(
      "buy_now_pay_later_info.get_redirect_url_response_"
      "info.base_failure_return_url");
  response.SetByDottedPath(
      "buy_now_pay_later_info.get_redirect_url_response_"
      "info.base_failure_return_url",
      "invalidfailureurl!!$$");
  request_->ParseResponse(response);

  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest,
       IsResponseComplete_ParseResponseCalled_NoContextToken) {
  Dict response = GetFullResponse();
  response.RemoveByDottedPath(
      "buy_now_pay_later_info.get_redirect_url_response_"
      "info.get_payment_instrument_context_token");
  request_->ParseResponse(response);

  EXPECT_FALSE(request_->IsResponseComplete());
}

TEST_F(GetBnplPaymentInstrumentForFetchingUrlRequestTest, RespondToDelegate) {
  Dict response_dict = Dict().Set(
      "buy_now_pay_later_info",
      Dict().Set(
          "get_redirect_url_response_info",
          Dict()
              .Set("redirect_url", "http://redirect-url.test/")
              .Set("base_success_return_url", "http://success-url.test/")
              .Set("base_failure_return_url", "http://failure-url.test/")
              .Set("get_payment_instrument_context_token", "CONTEXT_TOKEN")));

  request_->ParseResponse(response_dict);

  EXPECT_CALL(mock_callback_,
              Run(PaymentsRpcResult::kSuccess,
                  /*response_details=*/AllOf(
                      Field(&BnplFetchUrlResponseDetails::redirect_url,
                            GURL("http://redirect-url.test/")),
                      Field(&BnplFetchUrlResponseDetails::success_url_prefix,
                            GURL("http://success-url.test/")),
                      Field(&BnplFetchUrlResponseDetails::failure_url_prefix,
                            GURL("http://failure-url.test/")),
                      Field(&BnplFetchUrlResponseDetails::context_token,
                            "CONTEXT_TOKEN"))));

  request_->RespondToDelegate(PaymentsRpcResult::kSuccess);
}

}  // namespace autofill::payments
