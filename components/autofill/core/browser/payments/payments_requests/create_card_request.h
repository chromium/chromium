// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_CREATE_CARD_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_CREATE_CARD_REQUEST_H_

#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

class CreateCardRequest : public PaymentsRequest {
 public:
  CreateCardRequest(
      const UploadCardRequestDetails& request_details,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const std::string&)> callback);
  CreateCardRequest(const CreateCardRequest&) = delete;
  CreateCardRequest& operator=(const CreateCardRequest&) = delete;
  ~CreateCardRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;
  std::string GetHistogramName() const override;
  std::optional<base::TimeDelta> GetTimeout() const override;

  std::string GetInstrumentIdForTesting() const;

 private:
  bool contains_card_info_ = false;
  const UploadCardRequestDetails request_details_;
  std::string instrument_id_;
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                          const std::string&)>
      callback_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_CREATE_CARD_REQUEST_H_
