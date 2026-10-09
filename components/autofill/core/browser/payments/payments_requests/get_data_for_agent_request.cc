// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_data_for_agent_request.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

namespace {

constexpr char kGetDataForAgentRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/getdataforagent";

// Parses the JSON object that the server returns as a string in
// `payment_token`, e.g.
// {"paymentMethod": "CARD", "paymentMethodDetails": {"pan": "4111111111111111",
//  "cvc": "123", "expirationMonth": 12, "expirationYear": 2030}}
// Returns std::nullopt if the card number or the expiration date is missing.
std::optional<GetDataForAgentResponseDetails> ParsePaymentToken(
    std::string_view payment_token) {
  std::optional<base::DictValue> token =
      base::JSONReader::ReadDict(payment_token, base::JSON_PARSE_RFC);
  if (!token) {
    return std::nullopt;
  }
  const base::DictValue* details = token->FindDict("paymentMethodDetails");
  if (!details) {
    return std::nullopt;
  }
  const std::string* pan = details->FindString("pan");
  std::optional<int> month = details->FindInt("expirationMonth");
  std::optional<int> year = details->FindInt("expirationYear");
  if (!pan || pan->empty() || !month || *month < 1 || *month > 12 || !year ||
      *year <= 0) {
    return std::nullopt;
  }

  GetDataForAgentResponseDetails response_details;
  response_details.card_number = *pan;
  if (const std::string* cvc = details->FindString("cvc")) {
    response_details.cvc = *cvc;
  }
  response_details.expiration_month = *month;
  response_details.expiration_year = *year;
  return response_details;
}

}  // namespace

GetDataForAgentRequest::GetDataForAgentRequest(
    GetDataForAgentRequestDetails request_details,
    GetDataForAgentCallback callback)
    : request_details_(std::move(request_details)),
      callback_(std::move(callback)) {}

GetDataForAgentRequest::~GetDataForAgentRequest() = default;

std::string GetDataForAgentRequest::GetRequestUrlPath() {
  return kGetDataForAgentRequestPath;
}

std::string GetDataForAgentRequest::GetRequestContentType() {
  return "application/json";
}

std::string GetDataForAgentRequest::GetRequestContent() {
  // The opaque token grants access to the card. Do not log the request body.
  base::DictValue request_dict =
      base::DictValue()
          .Set("encrypted_token", request_details_.opaque_token)
          .Set("context", base::DictValue()
                              .Set("language_code", request_details_.app_locale)
                              .Set("billable_service",
                                   kUnmaskPaymentMethodBillableServiceNumber));
  return base::WriteJson(request_dict).value_or("");
}

void GetDataForAgentRequest::ParseResponse(const base::DictValue& response) {
  // Only `payment_token` is used. The test card in `mock_payment_token` is
  // ignored.
  // TODO(crbug.com/567655010): Support `agentic_payment_credentials`.
  const std::string* payment_token = response.FindString("payment_token");
  if (!payment_token) {
    return;
  }
  if (std::optional<GetDataForAgentResponseDetails> details =
          ParsePaymentToken(*payment_token)) {
    response_details_ = *std::move(details);
  }
}

bool GetDataForAgentRequest::IsResponseComplete() {
  return !response_details_.card_number.empty();
}

void GetDataForAgentRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, response_details_);
}

}  // namespace autofill::payments
