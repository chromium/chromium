// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_unmask_details_request.h"

#include <string>

#include "base/json/json_writer.h"

namespace autofill::payments {

namespace {
const char kGetUnmaskDetailsRequestPath[] =
    "payments/apis/chromepaymentsservice/getdetailsforgetrealpan";
}  // namespace

GetUnmaskDetailsRequest::GetUnmaskDetailsRequest(
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                            PaymentsNetworkInterface::UnmaskDetails&)> callback,
    const std::string& app_locale,
    const bool full_sync_enabled)
    : callback_(std::move(callback)),
      app_locale_(app_locale),
      full_sync_enabled_(full_sync_enabled) {}

GetUnmaskDetailsRequest::~GetUnmaskDetailsRequest() = default;

std::string GetUnmaskDetailsRequest::GetRequestUrlPath() {
  return kGetUnmaskDetailsRequestPath;
}

std::string GetUnmaskDetailsRequest::GetRequestContentType() {
  return "application/json";
}

std::string GetUnmaskDetailsRequest::GetRequestContent() {
  base::Value::Dict request_dict;
  base::Value::Dict context;
  context.Set("language_code", app_locale_);
  context.Set("billable_service", kUnmaskPaymentMethodBillableServiceNumber);
  request_dict.Set("context", std::move(context));

  base::Value::Dict chrome_user_context;
  chrome_user_context.Set("full_sync_enabled", full_sync_enabled_);
  request_dict.Set("chrome_user_context", std::move(chrome_user_context));

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  VLOG(3) << "getdetailsforgetrealpan request body: " << request_content;
  return request_content;
}

void GetUnmaskDetailsRequest::ParseResponse(const base::Value::Dict& response) {
  const auto* method = response.FindString("authentication_method");
  if (method) {
    if (*method == "CVC") {
      unmask_details_.unmask_auth_method =
          PaymentsAutofillClient::UnmaskAuthMethod::kCvc;
    } else if (*method == "FIDO") {
      unmask_details_.unmask_auth_method =
          PaymentsAutofillClient::UnmaskAuthMethod::kFido;
    }
  }

  const std::optional<bool> offer_fido_opt_in =
      response.FindBool("offer_fido_opt_in");
  unmask_details_.offer_fido_opt_in = offer_fido_opt_in.value_or(false);

  const base::Value::Dict* dictionary_value =
      response.FindDict("fido_request_options");
  if (dictionary_value)
    unmask_details_.fido_request_options = dictionary_value->Clone();

  const auto* fido_eligible_card_ids =
      response.FindList("fido_eligible_card_id");
  if (fido_eligible_card_ids) {
    for (const base::Value& result : *fido_eligible_card_ids) {
      unmask_details_.fido_eligible_card_ids.insert(result.GetString());
    }
  }
}

bool GetUnmaskDetailsRequest::IsResponseComplete() {
  return unmask_details_.unmask_auth_method !=
         PaymentsAutofillClient::UnmaskAuthMethod::kUnknown;
}

void GetUnmaskDetailsRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, unmask_details_);
}

}  // namespace autofill::payments
