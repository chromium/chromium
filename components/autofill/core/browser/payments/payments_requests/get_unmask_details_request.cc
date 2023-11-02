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
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            PaymentsClient::UnmaskDetails&)> callback,
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
  base::Value request_dict(base::Value::Type::DICTIONARY);
  base::Value context(base::Value::Type::DICTIONARY);
  context.SetKey("language_code", base::Value(app_locale_));
  context.SetKey("billable_service",
                 base::Value(kUnmaskCardBillableServiceNumber));
  request_dict.SetKey("context", std::move(context));

  base::Value chrome_user_context(base::Value::Type::DICTIONARY);
  chrome_user_context.SetKey("full_sync_enabled",
                             base::Value(full_sync_enabled_));
  request_dict.SetKey("chrome_user_context", std::move(chrome_user_context));

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  VLOG(3) << "getdetailsforgetrealpan request body: " << request_content;
  return request_content;
}

void GetUnmaskDetailsRequest::ParseResponse(const base::Value& response) {
  const auto* method = response.FindStringKey("authentication_method");
  if (method) {
    if (*method == "CVC") {
      unmask_details_.unmask_auth_method =
          AutofillClient::UnmaskAuthMethod::kCvc;
    } else if (*method == "FIDO") {
      unmask_details_.unmask_auth_method =
          AutofillClient::UnmaskAuthMethod::kFido;
    }
  }

  const auto* offer_fido_opt_in =
      response.FindKeyOfType("offer_fido_opt_in", base::Value::Type::BOOLEAN);
  unmask_details_.offer_fido_opt_in =
      offer_fido_opt_in && offer_fido_opt_in->GetBool();

  const auto* dictionary_value = response.FindKeyOfType(
      "fido_request_options", base::Value::Type::DICTIONARY);
  if (dictionary_value)
    unmask_details_.fido_request_options = dictionary_value->Clone();

  const auto* fido_eligible_card_ids =
      response.FindKeyOfType("fido_eligible_card_id", base::Value::Type::LIST);
  if (fido_eligible_card_ids) {
    for (const base::Value& result : fido_eligible_card_ids->GetList()) {
      unmask_details_.fido_eligible_card_ids.insert(result.GetString());
    }
  }
}

bool GetUnmaskDetailsRequest::IsResponseComplete() {
  return unmask_details_.unmask_auth_method !=
         AutofillClient::UnmaskAuthMethod::kUnknown;
}

void GetUnmaskDetailsRequest::RespondToDelegate(
    AutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, unmask_details_);
}

}  // namespace autofill::payments
