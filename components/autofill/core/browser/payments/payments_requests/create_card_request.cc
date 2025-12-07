// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/create_card_request.h"

#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request_constants.h"

namespace autofill::payments {

namespace {
const char kCreateCardRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/createpaymentinstrument"
    "?s7e_suffix=chromewallet";
const char kCreateCardRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_21_pan=%s&s7e_13_cvc=%s";
const char kCreateCardRequestFormatWithoutCvc[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_21_pan=%s";
}  // namespace

CreateCardRequest::CreateCardRequest(
    const UploadCardRequestDetails& request_details,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                            const std::string&)> callback)
    : request_details_(request_details), callback_(std::move(callback)) {}

CreateCardRequest::~CreateCardRequest() = default;

std::string CreateCardRequest::GetRequestUrlPath() {
  return kCreateCardRequestPath;
}

std::string CreateCardRequest::GetRequestContentType() {
  return "application/x-www-form-urlencoded";
}

std::string CreateCardRequest::GetRequestContent() {
  base::Value::Dict request_dict;

  const std::string& app_locale = request_details_.app_locale;
  base::Value::Dict context;
  context.Set("language_code", app_locale);
  context.Set("billable_service", kUploadPaymentMethodBillableServiceNumber);
  if (request_details_.billing_customer_number != 0) {
    context.Set("customer_context",
                BuildCustomerContextDictionary(
                    request_details_.billing_customer_number));
  }
  request_dict.Set("context", std::move(context));

  request_dict.Set(
      "chrome_user_context",
      BuildChromeUserContext(request_details_.client_behavior_signals));

  request_dict.Set("context_token", request_details_.context_token);

  if (request_details_.card.HasNonEmptyValidNickname()) {
    request_dict.Set("nickname", request_details_.card.nickname());
  }

  request_dict.Set("risk_data_encoded",
                   BuildRiskDictionary(request_details_.risk_data));

  base::Value::Dict card_info;
  card_info.Set("pan", "__param:s7e_21_pan");
  if (!request_details_.cvc.empty()) {
    card_info.Set("cvc", "__param:s7e_13_cvc");
  }
  int value = 0;
  const std::u16string exp_month =
      request_details_.card.GetInfo(CREDIT_CARD_EXP_MONTH, app_locale);
  const std::u16string exp_year =
      request_details_.card.GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, app_locale);
  if (base::StringToInt(exp_month, &value)) {
    card_info.Set("expiration_month", value);
  }
  if (base::StringToInt(exp_year, &value)) {
    card_info.Set("expiration_year", value);
  }
  SetStringIfNotEmpty(request_details_.card, CREDIT_CARD_NAME_FULL, app_locale,
                      "cardholder_name", card_info);
  // When this is invoked, caller is guaranteed to send only one unique address,
  // if not an empty list.
  CHECK_LE(request_details_.profiles.size(), 1U);
  if (!request_details_.profiles.empty()) {
    card_info.Set(
        "address",
        BuildAddressDictionary(request_details_.profiles[0], app_locale, true));
  }

  switch (request_details_.upload_card_source) {
    case UploadCardSource::kUnknown:
      card_info.Set("upload_card_source", "UNKNOWN_UPLOAD_CARD_SOURCE");
      break;
    case UploadCardSource::kUpstreamSaveAndFill:
      card_info.Set("upload_card_source", "UPSTREAM_SAVE_AND_FILL");
      break;
    default:
      // This class has not been integrated with other Upstream flows yet.
      NOTREACHED();
  }
  request_dict.Set("card_info", std::move(card_info));

  const std::u16string pan =
      request_details_.card.GetInfo(CREDIT_CARD_NUMBER, app_locale);
  std::string json_request = base::WriteJson(request_dict).value_or("");
  std::string request_content;
  if (request_details_.cvc.empty()) {
    request_content = base::StringPrintf(
        kCreateCardRequestFormatWithoutCvc,
        base::EscapeUrlEncodedData(json_request, true).c_str(),
        base::EscapeUrlEncodedData(base::UTF16ToASCII(pan), true).c_str());
  } else {
    request_content = base::StringPrintf(
        kCreateCardRequestFormat,
        base::EscapeUrlEncodedData(json_request, true).c_str(),
        base::EscapeUrlEncodedData(base::UTF16ToASCII(pan), true).c_str(),
        base::EscapeUrlEncodedData(base::UTF16ToASCII(request_details_.cvc),
                                   true)
            .c_str());
  }

  DVLOG(3) << "createcard request body: " << request_content;
  return request_content;
}

void CreateCardRequest::ParseResponse(const base::Value::Dict& response) {
  if (const base::Value::Dict* card_info = response.FindDict("card_info")) {
    contains_card_info_ = true;
    if (const std::string* instrument_id =
            card_info->FindString("instrument_id")) {
      instrument_id_ = std::move(*instrument_id);
    }
  }
}

bool CreateCardRequest::IsResponseComplete() {
  return contains_card_info_;
}

void CreateCardRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, instrument_id_);
}

std::string CreateCardRequest::GetHistogramName() const {
  return "CreateCardRequest";
}

std::optional<base::TimeDelta> CreateCardRequest::GetTimeout() const {
  return kUploadCardRequestTimeout;
}

std::string CreateCardRequest::GetInstrumentIdForTesting() const {
  return instrument_id_;
}

}  // namespace autofill::payments
