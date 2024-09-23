// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_UNMASK_DETAILS_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_UNMASK_DETAILS_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace base {
class Value;
}  // namespace base

namespace autofill::payments {

class GetUnmaskDetailsRequest : public PaymentsRequest {
 public:
  GetUnmaskDetailsRequest(
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              PaymentsNetworkInterface::UnmaskDetails&)>
          callback,
      const std::string& app_locale,
      const bool full_sync_enabled);
  GetUnmaskDetailsRequest(const GetUnmaskDetailsRequest&) = delete;
  GetUnmaskDetailsRequest& operator=(const GetUnmaskDetailsRequest&) = delete;
  ~GetUnmaskDetailsRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                          PaymentsNetworkInterface::UnmaskDetails&)>
      callback_;
  std::string app_locale_;
  const bool full_sync_enabled_;

  // Suggested authentication method and other information to facilitate card
  // unmasking.
  payments::PaymentsNetworkInterface::UnmaskDetails unmask_details_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_UNMASK_DETAILS_REQUEST_H_
