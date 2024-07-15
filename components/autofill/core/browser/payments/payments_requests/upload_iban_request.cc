// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/upload_iban_request.h"

#include "base/json/json_writer.h"
#include "base/strings/escape.h"

namespace autofill::payments {

namespace {

const char kUploadIbanRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/createpaymentinstrument"
    "?s7e_suffix=chromewallet";
const char kUploadIbanRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_443_value=%s";

}  // namespace

UploadIbanRequest::UploadIbanRequest(
    const PaymentsNetworkInterface::UploadIbanRequestDetails& details,
    bool full_sync_enabled,
    base::OnceCallback<
        void(payments::PaymentsAutofillClient::PaymentsRpcResult)> callback)
    : request_details_(details),
      full_sync_enabled_(full_sync_enabled),
      callback_(std::move(callback)) {}

UploadIbanRequest::~UploadIbanRequest() = default;

std::string UploadIbanRequest::GetRequestUrlPath() {
  return kUploadIbanRequestPath;
}

std::string UploadIbanRequest::GetRequestContentType() {
  return "application/x-www-form-urlencoded";
}

std::string UploadIbanRequest::GetRequestContent() {
  base::Value::Dict request_dict;

  base::Value::Dict iban_info;
  iban_info.Set("value", "__param:s7e_443_value");
  request_dict.Set("iban_info", std::move(iban_info));

  base::Value::Dict context;
  if (!request_details_.nickname.empty()) {
    context.Set("nickname", request_details_.nickname);
  }
  context.Set("language_code", request_details_.app_locale);
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
  request_dict.Set("context_token", request_details_.context_token);

  std::string json_request = base::WriteJson(request_dict).value();
  std::string request_content =
      base::StringPrintf(kUploadIbanRequestFormat,
                         base::EscapeUrlEncodedData(json_request, true).c_str(),
                         base::EscapeUrlEncodedData(
                             base::UTF16ToASCII(request_details_.value), true)
                             .c_str());
  VLOG(3) << "savediban request body: " << request_content;
  return request_content;
}

void UploadIbanRequest::ParseResponse(const base::Value::Dict& response) {
  NOTIMPLEMENTED();
}

bool UploadIbanRequest::IsResponseComplete() {
  return true;
}

void UploadIbanRequest::RespondToDelegate(
    payments::PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result);
}

}  // namespace autofill::payments
