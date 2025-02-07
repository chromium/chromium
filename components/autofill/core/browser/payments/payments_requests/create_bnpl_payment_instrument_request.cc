// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/create_bnpl_payment_instrument_request.h"

#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace autofill::payments {

namespace {
const char kCreateBnplPaymentInstrumentRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/createpaymentinstrument";

const char kCreateBnplPaymentInstrumentRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s";
}  // namespace

CreateBnplPaymentInstrumentRequest::CreateBnplPaymentInstrumentRequest(
    CreateBnplPaymentInstrumentRequestDetails request_details,
    bool full_sync_enabled,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                            std::u16string instrument_id)> callback)
    : request_details_(request_details),
      full_sync_enabled_(full_sync_enabled),
      callback_(std::move(callback)) {}

CreateBnplPaymentInstrumentRequest::~CreateBnplPaymentInstrumentRequest() =
    default;

std::string CreateBnplPaymentInstrumentRequest::GetRequestUrlPath() {
  return kCreateBnplPaymentInstrumentRequestPath;
}

std::string CreateBnplPaymentInstrumentRequest::GetRequestContentType() {
  return "application/x-www-form-urlencoded";
}

std::string CreateBnplPaymentInstrumentRequest::GetRequestContent() {
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

  request_dict.Set("context_token", request_details_.context_token);
  request_dict.Set("risk_data_encoded",
                   BuildRiskDictionary(request_details_.risk_data));

  std::string json_request = base::WriteJson(request_dict).value();
  std::string request_content = base::StringPrintf(
      kCreateBnplPaymentInstrumentRequestFormat,
      base::EscapeUrlEncodedData(json_request, true).c_str());
  VLOG(3) << "create bnpl payment instrument request body: " << request_content;
  return request_content;
}

void CreateBnplPaymentInstrumentRequest::ParseResponse(
    const base::Value::Dict& response) {
  if (const base::Value::Dict* buy_now_pay_later_info =
          response.FindDict("buy_now_pay_later_info")) {
    if (const std::string* instrument_id =
            buy_now_pay_later_info->FindString("instrument_id")) {
      instrument_id_ = base::UTF8ToUTF16(*instrument_id);
    }
  }
}

bool CreateBnplPaymentInstrumentRequest::IsResponseComplete() {
  return !instrument_id_.empty();
}

void CreateBnplPaymentInstrumentRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, std::move(instrument_id_));
}

}  // namespace autofill::payments
