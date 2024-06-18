// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/select_challenge_option_request.h"

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"

namespace autofill {
namespace payments {

namespace {
const char kSelectChallengeOptionRequestPath[] =
    "payments/apis/chromepaymentsservice/selectchallengeoption";
}  // namespace

SelectChallengeOptionRequest::SelectChallengeOptionRequest(
    PaymentsNetworkInterface::SelectChallengeOptionRequestDetails
        request_details,
    base::OnceCallback<void(payments::PaymentsAutofillClient::PaymentsRpcResult,
                            const std::string&)> callback)
    : request_details_(request_details), callback_(std::move(callback)) {}

SelectChallengeOptionRequest::~SelectChallengeOptionRequest() = default;

std::string SelectChallengeOptionRequest::GetRequestUrlPath() {
  return kSelectChallengeOptionRequestPath;
}

std::string SelectChallengeOptionRequest::GetRequestContentType() {
  return "application/json";
}

std::string SelectChallengeOptionRequest::GetRequestContent() {
  base::Value::Dict request_dict;
  base::Value::Dict context;
  context.Set("billable_service", kUnmaskPaymentMethodBillableServiceNumber);
  if (request_details_.billing_customer_number != 0) {
    context.Set("customer_context",
                BuildCustomerContextDictionary(
                    request_details_.billing_customer_number));
  }
  request_dict.Set("context", std::move(context));

  base::Value::Dict selected_idv_method;

  DCHECK_NE(request_details_.selected_challenge_option.type,
            CardUnmaskChallengeOptionType::kUnknownType);
  // Set if selected idv option is sms otp option.
  if (request_details_.selected_challenge_option.type ==
      CardUnmaskChallengeOptionType::kSmsOtp) {
    base::Value::Dict sms_challenge_option;
    // We only get and set the challenge id.
    if (!request_details_.selected_challenge_option.id.value().empty()) {
      sms_challenge_option.Set(
          "challenge_id",
          request_details_.selected_challenge_option.id.value());
    }
    selected_idv_method.Set("sms_otp_challenge_option",
                            std::move(sms_challenge_option));
  }
  if (request_details_.selected_challenge_option.type ==
      CardUnmaskChallengeOptionType::kEmailOtp) {
    base::Value::Dict email_challenge_option;
    // We only get and set the challenge id.
    if (!request_details_.selected_challenge_option.id.value().empty()) {
      email_challenge_option.Set(
          "challenge_id",
          request_details_.selected_challenge_option.id.value());
    }
    selected_idv_method.Set("email_otp_challenge_option",
                            std::move(email_challenge_option));
  }
  request_dict.Set("selected_idv_challenge_option",
                   std::move(selected_idv_method));

  if (!request_details_.context_token.empty()) {
    request_dict.Set("context_token", request_details_.context_token);
  }

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  VLOG(3) << "selectchallengeoption request body: " << request_content;
  return request_content;
}

void SelectChallengeOptionRequest::ParseResponse(
    const base::Value::Dict& response) {
  const std::string* updated_context_token =
      response.FindString("context_token");
  updated_context_token_ =
      updated_context_token ? *updated_context_token : std::string();
}

bool SelectChallengeOptionRequest::IsResponseComplete() {
  return !updated_context_token_.empty();
}

void SelectChallengeOptionRequest::RespondToDelegate(
    payments::PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, updated_context_token_);
}

}  // namespace payments
}  // namespace autofill
