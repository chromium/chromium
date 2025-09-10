// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_details_for_create_card_request.h"

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill::payments {

namespace {
const char kGetDetailsForCreateCardRequestPath[] =
    "payments/apis/chromepaymentsservice/getdetailsforcreatepaymentinstrument";
}  // namespace

GetDetailsForCreateCardRequest::GetDetailsForCreateCardRequest(
    const std::string& unique_country_code,
    const std::vector<ClientBehaviorConstants>& client_behavior_signals,
    const std::string& app_locale,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                            const std::u16string&,
                            std::unique_ptr<base::Value::Dict>,
                            std::vector<std::pair<int, int>>)> callback,
    const int billable_service_number,
    const int64_t billing_customer_number,
    UploadCardSource upload_card_source)
    : unique_country_code_(unique_country_code),
      client_behavior_signals_(client_behavior_signals),
      app_locale_(app_locale),
      callback_(std::move(callback)),
      billable_service_number_(billable_service_number),
      upload_card_source_(upload_card_source),
      billing_customer_number_(billing_customer_number) {}

GetDetailsForCreateCardRequest::~GetDetailsForCreateCardRequest() = default;

std::string GetDetailsForCreateCardRequest::GetRequestUrlPath() {
  return kGetDetailsForCreateCardRequestPath;
}

std::string GetDetailsForCreateCardRequest::GetRequestContentType() {
  return "application/json";
}

std::string GetDetailsForCreateCardRequest::GetRequestContent() {
  base::Value::Dict request_dict;
  base::Value::Dict context;
  context.Set("language_code", app_locale_);
  context.Set("billable_service", billable_service_number_);
  if (billing_customer_number_ != 0) {
    context.Set("customer_context",
                BuildCustomerContextDictionary(billing_customer_number_));
  }
  request_dict.Set("context", std::move(context));
  request_dict.Set("chrome_user_context",
                   BuildChromeUserContext(client_behavior_signals_));

  base::Value::Dict card_info;
  card_info.Set("unique_country_code", unique_country_code_);
  switch (upload_card_source_) {
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

  std::string request_content = base::WriteJson(request_dict).value_or("");
  DVLOG(3) << "getdetailsforcreatecard request body: " << request_content;
  return request_content;
}

void GetDetailsForCreateCardRequest::ParseResponse(
    const base::Value::Dict& response) {
  if (const auto* context_token = response.FindString("context_token")) {
    context_token_ = base::UTF8ToUTF16(*context_token);
  }

  if (const base::Value::Dict* dictionary_value =
          response.FindDict("legal_message")) {
    legal_message_ =
        std::make_unique<base::Value::Dict>(dictionary_value->Clone());
  }

  if (const base::Value::Dict* card_details =
          response.FindDict("card_details")) {
    if (const auto* supported_card_bin_ranges_string =
            card_details->FindString("supported_card_bin_ranges_string")) {
      supported_card_bin_ranges_ =
          ParseSupportedCardBinRangesString(*supported_card_bin_ranges_string);
    }
  }
}

bool GetDetailsForCreateCardRequest::IsResponseComplete() {
  return !context_token_.empty() && legal_message_;
}

void GetDetailsForCreateCardRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, context_token_, std::move(legal_message_),
                           supported_card_bin_ranges_);
}

}  // namespace autofill::payments
