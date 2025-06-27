// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_UPDATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_UPDATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

class GetDetailsForUpdateBnplPaymentInstrumentRequest : public PaymentsRequest {
 public:
  GetDetailsForUpdateBnplPaymentInstrumentRequest(
      GetDetailsForUpdateBnplPaymentInstrumentRequestDetails request_details,
      bool full_sync_enabled,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              std::string,
                              LegalMessageLines)> callback);
  GetDetailsForUpdateBnplPaymentInstrumentRequest(
      const GetDetailsForUpdateBnplPaymentInstrumentRequest&) = delete;
  GetDetailsForUpdateBnplPaymentInstrumentRequest& operator=(
      const GetDetailsForUpdateBnplPaymentInstrumentRequest&) = delete;
  ~GetDetailsForUpdateBnplPaymentInstrumentRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  friend class GetDetailsForUpdateBnplPaymentInstrumentRequestTestApi;

  GetDetailsForUpdateBnplPaymentInstrumentRequestDetails request_details_;
  bool full_sync_enabled_;
  std::string context_token_;
  LegalMessageLines legal_message_;
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                          std::string,
                          LegalMessageLines)>
      callback_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_DETAILS_FOR_UPDATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_H_
