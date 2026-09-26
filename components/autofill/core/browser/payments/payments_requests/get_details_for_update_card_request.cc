// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_details_for_update_card_request.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

namespace {
const char kGetDetailsForUpdateCardRequestPath[] =
    "payments/apis/chromepaymentsservice/getdetailsforupdatepaymentinstrument";
}  // namespace

GetDetailsForUpdateCardRequest::GetDetailsForUpdateCardRequest(
    GetDetailsForUpdateCardRequestDetails request_details,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                            const std::string&)> callback)
    : request_details_(std::move(request_details)),
      callback_(std::move(callback)) {}

GetDetailsForUpdateCardRequest::~GetDetailsForUpdateCardRequest() = default;

std::string GetDetailsForUpdateCardRequest::GetRequestUrlPath() {
  return kGetDetailsForUpdateCardRequestPath;
}

std::string GetDetailsForUpdateCardRequest::GetRequestContentType() {
  return "application/json";
}

std::string GetDetailsForUpdateCardRequest::GetRequestContent() {
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

  // Empty card_info dictionary indicates a credit card update preflight.
  request_dict.Set("card_info", base::DictValue());

  std::string request_content = base::WriteJson(request_dict).value_or("");
  DVLOG(3) << "getdetailsforupdatecard request body: " << request_content;
  return request_content;
}

void GetDetailsForUpdateCardRequest::ParseResponse(
    const base::DictValue& response) {
  if (const std::string* context_token = response.FindString("context_token")) {
    context_token_ = *context_token;
  }
}

bool GetDetailsForUpdateCardRequest::IsResponseComplete() {
  return !context_token_.empty();
}

void GetDetailsForUpdateCardRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, context_token_);
}

}  // namespace autofill::payments
