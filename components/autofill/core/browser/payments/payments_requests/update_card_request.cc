// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/update_card_request.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

namespace {
const char kUpdateCardRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/updatepaymentinstrument"
    "?s7e_suffix=chromewallet";

const char kUpdateCardRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_13_cvc=%s";
}  // namespace

UpdateCardRequest::UpdateCardRequest(
    UpdateCardRequestDetails request_details,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
        callback)
    : request_details_(std::move(request_details)),
      callback_(std::move(callback)) {}

UpdateCardRequest::~UpdateCardRequest() = default;

std::string UpdateCardRequest::GetRequestUrlPath() {
  return kUpdateCardRequestPath;
}

std::string UpdateCardRequest::GetRequestContentType() {
  return "application/x-www-form-urlencoded";
}

std::string UpdateCardRequest::GetRequestContent() {
  base::DictValue request_dict;

  base::DictValue context;
  context.Set("language_code", request_details_.app_locale);
  context.Set("billable_service", kUploadPaymentMethodBillableServiceNumber);
  if (request_details_.billing_customer_number != 0) {
    context.Set("customer_context",
                BuildCustomerContextDictionary(
                    request_details_.billing_customer_number));
  }
  request_dict.Set("context", std::move(context));

  request_dict.Set("chrome_user_context", BuildChromeUserContext());

  request_dict.Set("instrument_id",
                   base::NumberToString(request_details_.instrument_id));

  request_dict.Set("risk_data_encoded",
                   BuildRiskDictionary(request_details_.risk_data));

  request_dict.Set("context_token", request_details_.context_token);

  base::DictValue card_info;
  card_info.Set("cardholder_name",
                base::UTF16ToUTF8(request_details_.cardholder_name));
  card_info.Set("encrypted_cvc", "__param:s7e_13_cvc");
  request_dict.Set("card_info", std::move(card_info));

  std::string json_request = base::WriteJson(request_dict).value_or("");
  return base::StringPrintf(
      kUpdateCardRequestFormat, base::EscapeUrlEncodedData(json_request, true),
      base::EscapeUrlEncodedData(base::UTF16ToASCII(request_details_.cvc),
                                 true));
}

void UpdateCardRequest::ParseResponse(const base::DictValue& response) {
  has_card_info_ = response.FindDict("card_info") != nullptr;
}

bool UpdateCardRequest::IsResponseComplete() {
  return has_card_info_;
}

void UpdateCardRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result);
}

}  // namespace autofill::payments
