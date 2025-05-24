// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_details_for_create_bnpl_payment_instrument_request.h"

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace autofill::payments {

namespace {
const char kGetDetailsForCreateBnplPaymentInstrumentRequestPath[] =
    "payments/apis/chromepaymentsservice/getdetailsforcreatepaymentinstrument";
}  // namespace

GetDetailsForCreateBnplPaymentInstrumentRequest::
    GetDetailsForCreateBnplPaymentInstrumentRequest(
        GetDetailsForCreateBnplPaymentInstrumentRequestDetails request_details,
        bool full_sync_enabled,
        base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                                std::string context_token,
                                LegalMessageLines)> callback)
    : request_details_(request_details),
      full_sync_enabled_(full_sync_enabled),
      callback_(std::move(callback)) {}

GetDetailsForCreateBnplPaymentInstrumentRequest::
    ~GetDetailsForCreateBnplPaymentInstrumentRequest() = default;

std::string
GetDetailsForCreateBnplPaymentInstrumentRequest::GetRequestUrlPath() {
  return kGetDetailsForCreateBnplPaymentInstrumentRequestPath;
}

std::string
GetDetailsForCreateBnplPaymentInstrumentRequest::GetRequestContentType() {
  return "application/json";
}

std::string
GetDetailsForCreateBnplPaymentInstrumentRequest::GetRequestContent() {
  base::Value::Dict request_dict;
  base::Value::Dict context;
  context.Set("language_code", request_details_.app_locale);
  context.Set("billable_service",
              payments::kUploadPaymentMethodBillableServiceNumber);
  if (request_details_.billing_customer_number != 0) {
    context.Set("customer_context",
                BuildCustomerContextDictionary(
                    request_details_.billing_customer_number));
  }
  request_dict.Set("context", std::move(context));

  base::Value::Dict chrome_user_context;
  chrome_user_context.Set("full_sync_enabled", full_sync_enabled_);
  request_dict.Set("chrome_user_context", std::move(chrome_user_context));

  base::Value::Dict buy_now_pay_later_info;
  buy_now_pay_later_info.Set("issuer_id", request_details_.issuer_id);
  request_dict.Set("buy_now_pay_later_info", std::move(buy_now_pay_later_info));

  return base::WriteJson(request_dict).value();
}

void GetDetailsForCreateBnplPaymentInstrumentRequest::ParseResponse(
    const base::Value::Dict& response) {
  if (const std::string* context_token = response.FindString("context_token")) {
    context_token_ = context_token ? *context_token : std::string();
  }

  if (const base::Value::Dict* legal_message_value =
          response.FindDict("legal_message")) {
    LegalMessageLine::Parse(*legal_message_value, &legal_message_,
                            /*escape_apostrophes=*/true);
  }
}

bool GetDetailsForCreateBnplPaymentInstrumentRequest::IsResponseComplete() {
  return !context_token_.empty() && !legal_message_.empty();
}

void GetDetailsForCreateBnplPaymentInstrumentRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, std::move(context_token_),
                           std::move(legal_message_));
}

}  // namespace autofill::payments
