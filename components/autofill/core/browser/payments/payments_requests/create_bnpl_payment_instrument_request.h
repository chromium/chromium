// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_CREATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_CREATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

class CreateBnplPaymentInstrumentRequest : public PaymentsRequest {
 public:
  CreateBnplPaymentInstrumentRequest(
      CreateBnplPaymentInstrumentRequestDetails request_details,
      bool full_sync_enabled,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              std::string instrument_id)> callback);
  CreateBnplPaymentInstrumentRequest(
      const CreateBnplPaymentInstrumentRequest&) = delete;
  CreateBnplPaymentInstrumentRequest& operator=(
      const CreateBnplPaymentInstrumentRequest&) = delete;
  ~CreateBnplPaymentInstrumentRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  friend class CreateBnplPaymentInstrumentRequestTestApi;

  CreateBnplPaymentInstrumentRequestDetails request_details_;
  bool full_sync_enabled_;
  std::string instrument_id_;
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                          std::string instrument_id)>
      callback_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_CREATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_H_
