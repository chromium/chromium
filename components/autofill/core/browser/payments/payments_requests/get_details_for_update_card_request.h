// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_UPDATE_CARD_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_UPDATE_CARD_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

class GetDetailsForUpdateCardRequest : public PaymentsRequest {
 public:
  GetDetailsForUpdateCardRequest(
      GetDetailsForUpdateCardRequestDetails request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const std::string&)> callback);
  GetDetailsForUpdateCardRequest(const GetDetailsForUpdateCardRequest&) =
      delete;
  GetDetailsForUpdateCardRequest& operator=(
      const GetDetailsForUpdateCardRequest&) = delete;
  ~GetDetailsForUpdateCardRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::DictValue& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  friend class GetDetailsForUpdateCardRequestTestApi;

  GetDetailsForUpdateCardRequestDetails request_details_;
  std::string context_token_;
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                          const std::string&)>
      callback_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_UPDATE_CARD_REQUEST_H_
