// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_bnpl_payment_instrument_for_fetching_url_request.h"

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"

namespace autofill::payments {

namespace {
using Dict = base::Value::Dict;

const char kGetBnplPaymentInstrumentForFetchingUrlRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/getpaymentinstrument";
}  // namespace

GetBnplPaymentInstrumentForFetchingUrlRequest::
    GetBnplPaymentInstrumentForFetchingUrlRequest(
        GetBnplPaymentInstrumentForFetchingUrlRequestDetails request_details,
        bool full_sync_enabled,
        base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                                const BnplFetchUrlResponseDetails&)> callback)
    : request_details_(request_details),
      full_sync_enabled_(full_sync_enabled),
      callback_(std::move(callback)) {}

GetBnplPaymentInstrumentForFetchingUrlRequest::
    ~GetBnplPaymentInstrumentForFetchingUrlRequest() = default;

std::string GetBnplPaymentInstrumentForFetchingUrlRequest::GetRequestUrlPath() {
  return kGetBnplPaymentInstrumentForFetchingUrlRequestPath;
}

std::string
GetBnplPaymentInstrumentForFetchingUrlRequest::GetRequestContentType() {
  return "application/json";
}

std::string GetBnplPaymentInstrumentForFetchingUrlRequest::GetRequestContent() {
  Dict request_dict =
      Dict()
          .Set("context",
               Dict()
                   .Set("billable_service",
                        payments::kUnmaskPaymentMethodBillableServiceNumber)
                   .Set("customer_context",
                        BuildCustomerContextDictionary(
                            request_details_.billing_customer_number)))
          .Set("chrome_user_context",
               Dict().Set("full_sync_enabled", full_sync_enabled_))
          .Set("instrument_id", request_details_.instrument_id)
          .Set("risk_data_encoded",
               BuildRiskDictionary(request_details_.risk_data))
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

  return base::WriteJson(request_dict).value();
}

void GetBnplPaymentInstrumentForFetchingUrlRequest::ParseResponse(
    const Dict& response) {
  const Dict* bnpl_info = response.FindDict("buy_now_pay_later_info");
  if (!bnpl_info) {
    return;
  }

  const Dict* url_response_info =
      bnpl_info->FindDict("get_redirect_url_response_info");
  if (!url_response_info) {
    return;
  }

  const std::string* redirect_url =
      url_response_info->FindString("redirect_url");
  response_details_.redirect_url = redirect_url ? GURL(*redirect_url) : GURL();

  const std::string* success_url_prefix =
      url_response_info->FindString("base_success_return_url");
  response_details_.success_url_prefix =
      success_url_prefix ? GURL(*success_url_prefix) : GURL();

  const std::string* failure_url_prefix =
      url_response_info->FindString("base_failure_return_url");
  response_details_.failure_url_prefix =
      failure_url_prefix ? GURL(*failure_url_prefix) : GURL();

  const std::string* context_token =
      url_response_info->FindString("get_payment_instrument_context_token");
  response_details_.context_token =
      context_token ? *context_token : std::string();
}

bool GetBnplPaymentInstrumentForFetchingUrlRequest::IsResponseComplete() {
  return response_details_.redirect_url.is_valid() &&
         response_details_.success_url_prefix.is_valid() &&
         response_details_.failure_url_prefix.is_valid() &&
         !response_details_.context_token.empty();
}

void GetBnplPaymentInstrumentForFetchingUrlRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, response_details_);
}

}  // namespace autofill::payments
