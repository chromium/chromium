// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UNMASK_IBAN_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UNMASK_IBAN_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

class UnmaskIbanRequest : public PaymentsRequest {
 public:
  UnmaskIbanRequest(
      const PaymentsNetworkInterface::UnmaskIbanRequestDetails& request_details,
      bool full_sync_enabled,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const std::u16string&)> callback);
  UnmaskIbanRequest(const UnmaskIbanRequest&) = delete;
  UnmaskIbanRequest& operator=(const UnmaskIbanRequest&) = delete;
  ~UnmaskIbanRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

  const std::u16string& value_for_testing() const { return value_; }

 private:
  const PaymentsNetworkInterface::UnmaskIbanRequestDetails request_details_;
  const bool full_sync_enabled_;
  std::u16string value_;
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                          const std::u16string&)>
      callback_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UNMASK_IBAN_REQUEST_H_
