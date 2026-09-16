// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/network_api/get_details_for_ewallet_account_linking_request.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace payments::facilitated {

namespace {

const char kGetDetailsForCreateInstrumentPath[] =
    "payments/apis/chromepaymentsservice/getdetailsforcreatepaymentinstrument";

// Billable service number is defined in Payments server to distinguish
// different requests.
constexpr int kChromePaymentsBillableServiceNumber = 70073;

}  // namespace

GetDetailsForEwalletAccountLinkingRequest::
    GetDetailsForEwalletAccountLinkingRequest(
        int64_t billing_customer_number,
        const std::vector<uint8_t>& client_token,
        base::OnceCallback<
            void(autofill::payments::PaymentsAutofillClient::PaymentsRpcResult,
                 bool,
                 const std::vector<uint8_t>&)> response_callback,
        const std::string& app_locale,
        const bool full_sync_enabled,
        base::DictValue account_linking_payload)
    : billing_customer_number_(billing_customer_number),
      client_token_(client_token),
      response_callback_(std::move(response_callback)),
      app_locale_(app_locale),
      full_sync_enabled_(full_sync_enabled),
      account_linking_payload_(std::move(account_linking_payload)) {}

GetDetailsForEwalletAccountLinkingRequest::
    ~GetDetailsForEwalletAccountLinkingRequest() = default;

std::string GetDetailsForEwalletAccountLinkingRequest::GetRequestUrlPath() {
  return kGetDetailsForCreateInstrumentPath;
}

std::string GetDetailsForEwalletAccountLinkingRequest::GetRequestContentType() {
  return "application/json";
}

std::string GetDetailsForEwalletAccountLinkingRequest::GetRequestContent() {
  base::DictValue request_dict =
      base::DictValue()
          .Set("chrome_user_context",
               base::DictValue().Set("full_sync_enabled", full_sync_enabled_))
          .Set(
              "context",
              base::DictValue()
                  .Set("language_code", app_locale_)
                  .Set("billable_service", kChromePaymentsBillableServiceNumber)
                  .Set("customer_context",
                       base::DictValue().Set(
                           "external_customer_id",
                           base::NumberToString(billing_customer_number_))));

  const base::DictValue* ewallet_info =
      account_linking_payload_.FindDict("ewallet_account_linking_info");
  CHECK(ewallet_info);
  request_dict.Set("ewallet_account_linking_info", ewallet_info->Clone());

  if (!client_token_.empty()) {
    request_dict.Set("client_token", base::Base64Encode(client_token_));
  }

  return base::WriteJson(request_dict).value_or("");
}

void GetDetailsForEwalletAccountLinkingRequest::ParseResponse(
    const base::DictValue& response) {
  if (response.FindDict("error")) {
    return;
  }
  if (const base::DictValue* details_dict =
          response.FindDict("ewallet_account_linking_details")) {
    if (const std::string* action_token_b64 =
            details_dict->FindString("action_token")) {
      action_token_ = base::Base64Decode(*action_token_b64)
                          .value_or(std::vector<uint8_t>{});
      is_eligible_for_ewallet_account_linking_ = !action_token_.empty();
    }
  }
}

bool GetDetailsForEwalletAccountLinkingRequest::IsResponseComplete() {
  return true;
}

void GetDetailsForEwalletAccountLinkingRequest::RespondToDelegate(
    autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(response_callback_)
      .Run(result, is_eligible_for_ewallet_account_linking_, action_token_);
}

}  // namespace payments::facilitated
