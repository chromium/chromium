// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/record_legal_reminder_acknowledgment_request.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

namespace {

const char kRecordLegalReminderAcknowledgmentRequestPath[] =
    "payments/apis/chromepaymentsservice/recordlegalreminderacknowledgment";

}  // namespace

RecordLegalReminderAcknowledgmentRequest::
    RecordLegalReminderAcknowledgmentRequest(
        const RecordLegalReminderAcknowledgmentRequestDetails& request_details,
        base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
            callback)
    : request_details_(request_details), callback_(std::move(callback)) {}

RecordLegalReminderAcknowledgmentRequest::
    ~RecordLegalReminderAcknowledgmentRequest() = default;

std::string RecordLegalReminderAcknowledgmentRequest::GetRequestUrlPath() {
  return kRecordLegalReminderAcknowledgmentRequestPath;
}

std::string RecordLegalReminderAcknowledgmentRequest::GetRequestContentType() {
  return "application/json";
}

std::string RecordLegalReminderAcknowledgmentRequest::GetRequestContent() {
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
  request_dict.Set("legal_message_token",
                   request_details_.legal_message_token);

  return base::WriteJson(request_dict).value();
}

void RecordLegalReminderAcknowledgmentRequest::ParseResponse(
    const base::DictValue& response) {
  // Empty response indicates success.
}

bool RecordLegalReminderAcknowledgmentRequest::IsResponseComplete() {
  return true;
}

void RecordLegalReminderAcknowledgmentRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result);
}

}  // namespace autofill::payments
