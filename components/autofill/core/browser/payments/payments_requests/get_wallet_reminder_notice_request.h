// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_WALLET_REMINDER_NOTICE_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_WALLET_REMINDER_NOTICE_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

// Payments request to get the Wallet reminder notice legal message and
// acknowledgment token, or check if the user has already acknowledged.
class GetWalletReminderNoticeRequest : public PaymentsRequest {
 public:
  GetWalletReminderNoticeRequest(
      const GetWalletReminderNoticeRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const GetWalletReminderNoticeResponseDetails&)>
          callback);
  GetWalletReminderNoticeRequest(const GetWalletReminderNoticeRequest&) =
      delete;
  GetWalletReminderNoticeRequest& operator=(
      const GetWalletReminderNoticeRequest&) = delete;
  ~GetWalletReminderNoticeRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::DictValue& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  friend class GetWalletReminderNoticeRequestTest;
  friend class GetWalletReminderNoticeRequestTestApi;

  // Used to store information to be populated to the request.
  GetWalletReminderNoticeRequestDetails request_details_;

  // Used to store information parsed from the response. Will be passed into the
  // `callback_` function as a param.
  GetWalletReminderNoticeResponseDetails response_details_;

  // The callback function to be invoked when the response is received.
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                          const GetWalletReminderNoticeResponseDetails&)>
      callback_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_WALLET_REMINDER_NOTICE_REQUEST_H_
