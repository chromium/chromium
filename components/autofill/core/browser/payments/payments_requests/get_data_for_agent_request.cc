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

// Returns the response details, or std::nullopt if the card number or the
// expiration date is missing.
std::optional<GetDataForAgentResponseDetails> CreateResponseDetails(
    const std::string* card_number,
    const std::string* cvc,
    std::optional<int> expiration_month,
    std::optional<int> expiration_year) {
  if (!card_number || card_number->empty() || !expiration_month ||
      *expiration_month < 1 || *expiration_month > 12 || !expiration_year ||
      *expiration_year <= 0) {
    return std::nullopt;
  }

  GetDataForAgentResponseDetails response_details;
  response_details.card_number = *card_number;
  if (cvc) {
    response_details.cvc = *cvc;
  }
  response_details.expiration_month = *expiration_month;
  response_details.expiration_year = *expiration_year;
  return response_details;
}

// Parses the JSON object that the server returns as a string in
// `payment_token` for a card number (FPAN), e.g.
// {"paymentMethod": "CARD", "paymentMethodDetails": {"pan": "4111111111111111",
//  "cvc": "123", "expirationMonth": 12, "expirationYear": 2030}}
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
  return CreateResponseDetails(
      details->FindString("pan"), details->FindString("cvc"),
      details->FindInt("expirationMonth"), details->FindInt("expirationYear"));
}

// Parses `agentic_payment_credentials`, which the server returns for a network
// agentic token (APAN), e.g.
// {"token": "5555555555554444", "cvv": "456", "expiration_month": 7,
//  "expiration_year": 2031}
// The token is filled like a card number, and the CVV is a dynamic CVV.
std::optional<GetDataForAgentResponseDetails> ParseAgenticPaymentCredentials(
    const base::DictValue& credentials) {
  return CreateResponseDetails(credentials.FindString("token"),
                               credentials.FindString("cvv"),
                               credentials.FindInt("expiration_month"),
                               credentials.FindInt("expiration_year"));
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
  // The server sets either `payment_token` or `agentic_payment_credentials`,
  // depending on the type of credential. The test card in `mock_payment_token`
  // is ignored.
  std::optional<GetDataForAgentResponseDetails> details;
  if (const std::string* payment_token = response.FindString("payment_token")) {
    details = ParsePaymentToken(*payment_token);
  } else if (const base::DictValue* credentials =
                 response.FindDict("agentic_payment_credentials")) {
    details = ParseAgenticPaymentCredentials(*credentials);
  }
  if (details) {
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
