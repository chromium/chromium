// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_bnpl_payment_instrument_for_fetching_vcn_request.h"

#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace autofill::payments {

namespace {
using Dict = base::Value::Dict;

const char kGetBnplPaymentInstrumentForFetchingVcnRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/"
    "getpaymentinstrument?s7e_suffix=chromewallet";
const char kGetBnplPaymentInstrumentForFetchingVcnRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s";
}  // namespace

GetBnplPaymentInstrumentForFetchingVcnRequest::
    GetBnplPaymentInstrumentForFetchingVcnRequest(
        GetBnplPaymentInstrumentForFetchingVcnRequestDetails request_details,
        bool full_sync_enabled,
        base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                                const BnplFetchVcnResponseDetails&)> callback)
    : request_details_(request_details),
      full_sync_enabled_(full_sync_enabled),
      callback_(std::move(callback)) {}

GetBnplPaymentInstrumentForFetchingVcnRequest::
    ~GetBnplPaymentInstrumentForFetchingVcnRequest() = default;

std::string GetBnplPaymentInstrumentForFetchingVcnRequest::GetRequestUrlPath() {
  return kGetBnplPaymentInstrumentForFetchingVcnRequestPath;
}

std::string
GetBnplPaymentInstrumentForFetchingVcnRequest::GetRequestContentType() {
  return "application/x-www-form-urlencoded";
}

std::string GetBnplPaymentInstrumentForFetchingVcnRequest::GetRequestContent() {
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
               Dict().Set("retrieve_buy_now_pay_later_vcn_request_info",
                          Dict()
                              .Set("get_payment_instrument_context_token",
                                   request_details_.context_token)
                              .Set("redirect_response_url",
                                   request_details_.redirect_url.spec())
                              .Set("issuer_id", request_details_.issuer_id)));

  return base::StringPrintf(
      kGetBnplPaymentInstrumentForFetchingVcnRequestFormat,
      base::EscapeUrlEncodedData(base::WriteJson(request_dict).value(),
                                 /*use_plus=*/true)
          .c_str());
}

void GetBnplPaymentInstrumentForFetchingVcnRequest::ParseResponse(
    const Dict& response) {
  const Dict* bnpl_value = response.FindDict("buy_now_pay_later_info");
  if (!bnpl_value) {
    return;
  }

  const Dict* vcn_response_value =
      bnpl_value->FindDict("get_vcn_response_info");
  if (!vcn_response_value) {
    return;
  }

  const Dict* card = vcn_response_value->FindDict("virtual_card_info");
  if (!card) {
    return;
  }

  const std::string* pan = card->FindString("pan");
  response_details_.pan = pan ? *pan : std::string();

  const std::string* cvv = card->FindString("cvv");
  response_details_.cvv = cvv ? *cvv : std::string();

  const Dict* expiration = card->FindDict("expiration");
  if (expiration) {
    if (std::optional<int> month = expiration->FindInt("month")) {
      response_details_.expiration_month = base::NumberToString(month.value());
    }

    if (std::optional<int> year = expiration->FindInt("year")) {
      response_details_.expiration_year = base::NumberToString(year.value());
    }
  }

  const std::string* name = card->FindString("cardholder_name");
  response_details_.cardholder_name = name ? *name : std::string();
}

bool GetBnplPaymentInstrumentForFetchingVcnRequest::IsResponseComplete() {
  return !response_details_.pan.empty() && !response_details_.cvv.empty() &&
         !response_details_.expiration_month.empty() &&
         !response_details_.expiration_year.empty() &&
         !response_details_.cardholder_name.empty();
}

void GetBnplPaymentInstrumentForFetchingVcnRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, response_details_);
}

}  // namespace autofill::payments
