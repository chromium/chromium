// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DATA_FOR_AGENT_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DATA_FOR_AGENT_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

// Retrieves the card credentials that Google Payments associated with an
// opaque token. Google Payments issues the token after the user approved a
// purchase that an agent completes on the user's behalf.
class GetDataForAgentRequest : public PaymentsRequest {
 public:
  using GetDataForAgentCallback =
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const GetDataForAgentResponseDetails&)>;

  GetDataForAgentRequest(GetDataForAgentRequestDetails request_details,
                         GetDataForAgentCallback callback);
  GetDataForAgentRequest(const GetDataForAgentRequest&) = delete;
  GetDataForAgentRequest& operator=(const GetDataForAgentRequest&) = delete;
  ~GetDataForAgentRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::DictValue& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  const GetDataForAgentRequestDetails request_details_;
  GetDataForAgentCallback callback_;
  GetDataForAgentResponseDetails response_details_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DATA_FOR_AGENT_REQUEST_H_
