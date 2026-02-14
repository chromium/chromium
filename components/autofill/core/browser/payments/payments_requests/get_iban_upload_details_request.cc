// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_iban_upload_details_request.h"

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace autofill::payments {

namespace {
const char kGetIbanUploadDetailsRequestPath[] =
    "payments/apis/chromepaymentsservice/getdetailsforcreatepaymentinstrument";
}  // namespace

GetIbanUploadDetailsRequest::GetIbanUploadDetailsRequest(
    const bool full_sync_enabled,
    const std::string& app_locale,
    const std::vector<ClientBehaviorConstants>& client_behavior_signals,
    int64_t billing_customer_number,
    const std::string& country_code,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                            const std::u16string& validation_regex,
                            const std::u16string& context_token,
                            std::unique_ptr<base::DictValue>)> callback)
    : full_sync_enabled_(full_sync_enabled),
      app_locale_(app_locale),
      client_behavior_signals_(client_behavior_signals),
      billing_customer_number_(billing_customer_number),
      country_code_(country_code),
      callback_(std::move(callback)) {}

GetIbanUploadDetailsRequest::~GetIbanUploadDetailsRequest() = default;

std::string GetIbanUploadDetailsRequest::GetRequestUrlPath() {
  return kGetIbanUploadDetailsRequestPath;
}

std::string GetIbanUploadDetailsRequest::GetRequestContentType() {
  return "application/json";
}

std::string GetIbanUploadDetailsRequest::GetRequestContent() {
  base::DictValue request_dict;
  base::DictValue context;
  context.Set("language_code", app_locale_);
  context.Set("billable_service",
              payments::kUploadPaymentMethodBillableServiceNumber);
  if (billing_customer_number_ != 0) {
    context.Set("customer_context",
                BuildCustomerContextDictionary(billing_customer_number_));
  }
  request_dict.Set("context", std::move(context));
  request_dict.Set(
      "chrome_user_context",
      BuildChromeUserContext(client_behavior_signals_, full_sync_enabled_));

  base::DictValue iban_info;
  iban_info.Set("iban_region_code", country_code_);
  request_dict.Set("iban_info", std::move(iban_info));

  return base::WriteJson(request_dict).value();
}

void GetIbanUploadDetailsRequest::ParseResponse(
    const base::DictValue& response) {
  if (const base::DictValue* iban_details = response.FindDict("iban_details")) {
    if (const std::string* validation_regex =
            iban_details->FindString("validation_regex")) {
      validation_regex_ = base::UTF8ToUTF16(*validation_regex);
    }
  }

  if (const std::string* context_token = response.FindString("context_token")) {
    context_token_ = base::UTF8ToUTF16(*context_token);
  }

  if (const base::DictValue* legal_message_value =
          response.FindDict("legal_message")) {
    legal_message_ =
        std::make_unique<base::DictValue>(legal_message_value->Clone());
  }
}

bool GetIbanUploadDetailsRequest::IsResponseComplete() {
  return !validation_regex_.empty() && !context_token_.empty() &&
         legal_message_;
}

void GetIbanUploadDetailsRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, validation_regex_, context_token_,
                           std::move(legal_message_));
}

}  // namespace autofill::payments
