// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace payments::facilitated {

namespace {
const char kInitiatePaymentRequestPath[] =
    "payments/apis/chromepaymentsservice/initiatepayment";
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
      full_sync_enabled_(full_sync_enabled) {}

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
    // enum. In future, some other payment rail could also support PIX codes.
    payment_details.Set("payment_rail", "PIX");
    payment_details.Set("qr_code", request_details_->pix_code_.value());
  }
  // The request should have a payment rail.
  DCHECK(payment_details.FindString("payment_rail"));
  request_dict.Set("payment_details", std::move(payment_details));

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  return request_content;
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

  if (const base::Value::Dict* trigger_purchase_manager =
          response.FindDict("trigger_purchase_manager")) {
    if (const std::string* action_token =
            trigger_purchase_manager->FindString("o2_action_token")) {
      std::optional<std::vector<uint8_t>> decoded_bytes =
          base::Base64Decode(*action_token);
      if (decoded_bytes.has_value()) {
        response_details_->action_token_ = std::move(*decoded_bytes);
      }
    }
  }
}

bool FacilitatedPaymentsInitiatePaymentRequest::IsResponseComplete() {
  return !response_details_->action_token_.empty();
}

void FacilitatedPaymentsInitiatePaymentRequest::RespondToDelegate(
    autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(response_callback_).Run(result, std::move(response_details_));
}

}  // namespace payments::facilitated
