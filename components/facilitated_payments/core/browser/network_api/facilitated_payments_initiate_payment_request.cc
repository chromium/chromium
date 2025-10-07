// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace payments::facilitated {

namespace {

const char kInitiatePaymentRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/initiatepayment";

// Billable service number is defined in Payments server to distinguish
// different requests.
constexpr int kFacilitatedPaymentsBillableServiceNumber = 70154;

}  // namespace

FacilitatedPaymentsInitiatePaymentRequest::
    FacilitatedPaymentsInitiatePaymentRequest(
        std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
            request_details,
        FacilitatedPaymentsNetworkInterface::InitiatePaymentResponseCallback
            response_callback,
        const std::string& app_locale,
        const bool full_sync_enabled)
    : request_details_(std::move(request_details)),
      response_details_(std::make_unique<
                        FacilitatedPaymentsInitiatePaymentResponseDetails>()),
      response_callback_(std::move(response_callback)),
      app_locale_(app_locale),
      full_sync_enabled_(full_sync_enabled) {
  CHECK(!request_details_->payment_link_.empty() ||
        (request_details_->pix_code_.has_value() &&
         !request_details_->pix_code_.value().empty()));
}

FacilitatedPaymentsInitiatePaymentRequest::
    ~FacilitatedPaymentsInitiatePaymentRequest() = default;

std::string FacilitatedPaymentsInitiatePaymentRequest::GetRequestUrlPath() {
  return kInitiatePaymentRequestPath;
}

std::string FacilitatedPaymentsInitiatePaymentRequest::GetRequestContentType() {
  return "application/json";
}

std::string FacilitatedPaymentsInitiatePaymentRequest::GetRequestContent() {
  base::Value::Dict request_dict;

  base::Value::Dict chrome_user_context;
  chrome_user_context.Set("full_sync_enabled", full_sync_enabled_);
  request_dict.Set("chrome_user_context", std::move(chrome_user_context));

  base::Value::Dict risk_data;
  risk_data.Set("message_type", "BROWSER_NATIVE_FINGERPRINTING");
  risk_data.Set("encoding_type", "BASE_64");
  risk_data.Set("value", request_details_->risk_data_);
  request_dict.Set("risk_data_encoded", std::move(risk_data));

  request_dict.Set(
      "client_token",
      base::Value(base::Base64Encode(request_details_->client_token_)));

  base::Value::Dict context;
  context.Set("language_code", app_locale_);
  context.Set("billable_service", kFacilitatedPaymentsBillableServiceNumber);
  if (request_details_->billing_customer_number_.has_value()) {
    base::Value::Dict customer_context;
    customer_context.Set(
        "external_customer_id",
        base::NumberToString(
            request_details_->billing_customer_number_.value()));
    context.Set("customer_context", std::move(customer_context));
  }
  request_dict.Set("context", std::move(context));

  if (request_details_->merchant_payment_page_hostname_.has_value()) {
    base::Value::Dict merchant_info;
    merchant_info.Set(
        "merchant_checkout_page_url",
        request_details_->merchant_payment_page_hostname_.value());
    request_dict.Set("merchant_info", std::move(merchant_info));
  }

  request_dict.Set(
      "sender_instrument_id",
      base::NumberToString(request_details_->instrument_id_.value()));

  base::Value::Dict payment_details;
  if (request_details_->pix_code_.has_value()) {
    // TODO(b/332602034): Pass the payment rail to be used for payment as an
    // enum. In future, some other payment rail could also support Pix codes.
    payment_details.Set("payment_rail", "PIX");
    payment_details.Set("qr_code", request_details_->pix_code_.value());
  }
  if (!request_details_->payment_link_.empty()) {
    payment_details.Set("payment_rail", "PAYMENT_HYPERLINK");
    payment_details.Set("payment_hyperlink", request_details_->payment_link_);
  }
  request_dict.Set("payment_details", std::move(payment_details));

  return base::WriteJson(request_dict).value_or("");
}

void FacilitatedPaymentsInitiatePaymentRequest::ParseResponse(
    const base::Value::Dict& response) {
  // The `error` is only set in the absence of the action token.
  if (const base::Value::Dict* error = response.FindDict("error")) {
    if (const std::string* error_message =
            error->FindString("user_error_message")) {
      response_details_->error_message_ = *error_message;
    }
    return;
  }
  const base::Value::Dict* trigger_purchase_manager =
      response.FindDict("trigger_purchase_manager");
  if (!trigger_purchase_manager) {
    return;
  }
  const base::Value::Dict* secure_payload_json =
      trigger_purchase_manager->FindDict("secure_payload");
  if (!secure_payload_json) {
    return;
  }

  // Extract the action token and set it on the response. The action token is
  // required to trigger purchase manager, thus if the parsing fails at point,
  // simply return.
  const std::string* action_token =
      secure_payload_json->FindString("opaque_token");
  if (!action_token) {
    return;
  }
  std::optional<std::vector<uint8_t>> decoded_bytes =
      base::Base64Decode(*action_token);
  if (!decoded_bytes.has_value()) {
    return;
  }
  SecurePayload secure_payload;
  secure_payload.action_token = std::move(*decoded_bytes);

  // Extract the secure data and set it on the response. The secure data is an
  // optional field.
  const auto* response_secure_data_list =
      secure_payload_json->FindList("secure_data");
  if (response_secure_data_list) {
    std::vector<SecureData> secure_data;
    for (const base::Value& secure_data_json : *response_secure_data_list) {
      std::optional<int> key = secure_data_json.GetDict().FindInt("key");
      const std::string* value = secure_data_json.GetDict().FindString("value");
      if (key.has_value() && value) {
        secure_data.emplace_back(*key, *value);
      }
    }
    secure_payload.secure_data = std::move(secure_data);
  }

  response_details_->secure_payload_ = std::move(secure_payload);
}

bool FacilitatedPaymentsInitiatePaymentRequest::IsResponseComplete() {
  return !response_details_->secure_payload_.action_token.empty();
}

void FacilitatedPaymentsInitiatePaymentRequest::RespondToDelegate(
    autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(response_callback_).Run(result, std::move(response_details_));
}

}  // namespace payments::facilitated
