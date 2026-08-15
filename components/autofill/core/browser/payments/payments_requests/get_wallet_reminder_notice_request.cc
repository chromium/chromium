// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_wallet_reminder_notice_request.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

namespace {

const char kGetWalletReminderNoticeRequestPath[] =
    "payments/apis/chromepaymentsservice/getwalletremindernotice";

}  // namespace

GetWalletReminderNoticeRequest::GetWalletReminderNoticeRequest(
    const GetWalletReminderNoticeRequestDetails& request_details,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                            const GetWalletReminderNoticeResponseDetails&)>
        callback)
    : request_details_(request_details), callback_(std::move(callback)) {}

GetWalletReminderNoticeRequest::~GetWalletReminderNoticeRequest() = default;

std::string GetWalletReminderNoticeRequest::GetRequestUrlPath() {
  return kGetWalletReminderNoticeRequestPath;
}

std::string GetWalletReminderNoticeRequest::GetRequestContentType() {
  return "application/json";
}

std::string GetWalletReminderNoticeRequest::GetRequestContent() {
  base::DictValue context;
  context.Set("language_code", request_details_.app_locale);
  context.Set("billable_service", request_details_.billable_service_number);
  if (request_details_.billing_customer_number != 0) {
    context.Set("customer_context",
                BuildCustomerContextDictionary(
                    request_details_.billing_customer_number));
  }

  base::DictValue request_dict;
  request_dict.Set("context", std::move(context));

  return base::WriteJson(request_dict).value();
}

void GetWalletReminderNoticeRequest::ParseResponse(
    const base::DictValue& response) {
  if (const base::DictValue* legal_message =
          response.FindDict("legal_message")) {
    LegalMessageLine::Parse(*legal_message,
                            &response_details_.legal_message_lines,
                            /*escape_apostrophes=*/true);
    if (const std::string* acknowledgement_token =
            legal_message->FindString("token")) {
      response_details_.acknowledgement_token = *acknowledgement_token;
    }
  }

  response_details_.has_user_been_shown_reminder =
      response.FindBool("has_user_been_shown_reminder").value_or(false);
}

bool GetWalletReminderNoticeRequest::IsResponseComplete() {
  return response_details_.has_user_been_shown_reminder ||
         (!response_details_.legal_message_lines.empty() &&
          !response_details_.acknowledgement_token.empty());
}

void GetWalletReminderNoticeRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, response_details_);
}

}  // namespace autofill::payments
