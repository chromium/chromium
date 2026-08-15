// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_RECORD_LEGAL_REMINDER_ACKNOWLEDGMENT_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_RECORD_LEGAL_REMINDER_ACKNOWLEDGMENT_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

// Payments request to record the user acknowledgment after the Wallet reminder
// notice is shown.
class RecordLegalReminderAcknowledgmentRequest : public PaymentsRequest {
 public:
  RecordLegalReminderAcknowledgmentRequest(
      const RecordLegalReminderAcknowledgmentRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
          callback);
  RecordLegalReminderAcknowledgmentRequest(
      const RecordLegalReminderAcknowledgmentRequest&) = delete;
  RecordLegalReminderAcknowledgmentRequest& operator=(
      const RecordLegalReminderAcknowledgmentRequest&) = delete;
  ~RecordLegalReminderAcknowledgmentRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::DictValue& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  friend class RecordLegalReminderAcknowledgmentRequestTest;

  // Used to store information to be populated in the request.
  RecordLegalReminderAcknowledgmentRequestDetails request_details_;

  // The callback function to be invoked when the response is received.
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)> callback_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_RECORD_LEGAL_REMINDER_ACKNOWLEDGMENT_REQUEST_H_
