// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_SELECT_CHALLENGE_OPTION_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_SELECT_CHALLENGE_OPTION_REQUEST_H_

#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill {
namespace payments {

class SelectChallengeOptionRequest : public PaymentsRequest {
 public:
  SelectChallengeOptionRequest(
      PaymentsNetworkInterface::SelectChallengeOptionRequestDetails
          request_details,
      base::OnceCallback<
          void(payments::PaymentsAutofillClient::PaymentsRpcResult,
               const std::string&)> callback);
  ~SelectChallengeOptionRequest() override;
  SelectChallengeOptionRequest(const SelectChallengeOptionRequest&) = delete;
  SelectChallengeOptionRequest& operator=(const SelectChallengeOptionRequest&) =
      delete;

  // PaymentsRequest.
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      payments::PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  PaymentsNetworkInterface::SelectChallengeOptionRequestDetails
      request_details_;
  base::OnceCallback<void(payments::PaymentsAutofillClient::PaymentsRpcResult,
                          const std::string&)>
      callback_;

  std::string updated_context_token_;
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_SELECT_CHALLENGE_OPTION_REQUEST_H_
