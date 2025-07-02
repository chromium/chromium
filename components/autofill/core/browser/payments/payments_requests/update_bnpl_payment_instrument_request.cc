// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/update_bnpl_payment_instrument_request.h"

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace autofill::payments {

namespace {
const char kUpdateBnplPaymentInstrumentRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/updatepaymentinstrument";
}  // namespace

UpdateBnplPaymentInstrumentRequest::UpdateBnplPaymentInstrumentRequest(
    UpdateBnplPaymentInstrumentRequestDetails request_details,
    bool full_sync_enabled,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
        callback)
    : request_details_(std::move(request_details)),
      full_sync_enabled_(full_sync_enabled),
      callback_(std::move(callback)) {}

UpdateBnplPaymentInstrumentRequest::~UpdateBnplPaymentInstrumentRequest() =
    default;

std::string UpdateBnplPaymentInstrumentRequest::GetRequestUrlPath() {
  return kUpdateBnplPaymentInstrumentRequestPath;
}

std::string UpdateBnplPaymentInstrumentRequest::GetRequestContentType() {
  return "application/json";
}

std::string UpdateBnplPaymentInstrumentRequest::GetRequestContent() {
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

  request_dict.Set("instrument_id",
                   base::NumberToString(request_details_.instrument_id));

  base::Value::Dict buy_now_pay_later_info;
  buy_now_pay_later_info.Set("type", static_cast<int>(request_details_.type));
  buy_now_pay_later_info.Set("issuer_id", request_details_.issuer_id);
  request_dict.Set("buy_now_pay_later_info", std::move(buy_now_pay_later_info));

  request_dict.Set("context_token", request_details_.context_token);
  request_dict.Set("risk_data_encoded",
                   BuildRiskDictionary(request_details_.risk_data));

  return base::WriteJson(request_dict).value();
}

void UpdateBnplPaymentInstrumentRequest::ParseResponse(
    const base::Value::Dict& response) {
  received_buy_now_pay_later_info_ =
      response.FindDict("buy_now_pay_later_info") != nullptr;
}

bool UpdateBnplPaymentInstrumentRequest::IsResponseComplete() {
  return received_buy_now_pay_later_info_;
}

void UpdateBnplPaymentInstrumentRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result);
}

}  // namespace autofill::payments
