// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/unmask_iban_request.h"

#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill::payments {

namespace {
const char kUnmaskIbanRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/"
    "getpaymentinstrument?s7e_suffix=chromewallet";

const char kUnmaskIbanRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s";
}  // namespace

UnmaskIbanRequest::UnmaskIbanRequest(
    const PaymentsNetworkInterface::UnmaskIbanRequestDetails& request_details,
    bool full_sync_enabled,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                            const std::u16string&)> callback)
    : request_details_(request_details),
      full_sync_enabled_(full_sync_enabled),
      callback_(std::move(callback)) {}

UnmaskIbanRequest::~UnmaskIbanRequest() = default;

std::string UnmaskIbanRequest::GetRequestUrlPath() {
  return kUnmaskIbanRequestPath;
}

std::string UnmaskIbanRequest::GetRequestContentType() {
  return "application/x-www-form-urlencoded";
}

std::string UnmaskIbanRequest::GetRequestContent() {
  base::Value::Dict request_dict;
  base::Value::Dict context;
  context.Set("billable_service", request_details_.billable_service_number);
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

  // iban_info must always be set, even if blank, so that the Payments server
  // knows this is an UnmaskIbanRequest.
  base::Value::Dict iban_info;
  request_dict.Set("iban_info", std::move(iban_info));

  std::string json_request = base::WriteJson(request_dict).value();
  std::string request_content = base::StringPrintf(
      kUnmaskIbanRequestFormat,
      base::EscapeUrlEncodedData(json_request, true).c_str());
  return request_content;
}

void UnmaskIbanRequest::ParseResponse(const base::Value::Dict& response) {
  if (const base::Value::Dict* iban_info = response.FindDict("iban_info")) {
    if (const std::string* value = iban_info->FindString("value")) {
      value_ = base::UTF8ToUTF16(*value);
    }
  }
}

bool UnmaskIbanRequest::IsResponseComplete() {
  return !value_.empty();
}

void UnmaskIbanRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, value_);
}

}  // namespace autofill::payments
