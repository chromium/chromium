// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_BNPL_PAYMENT_INSTRUMENT_FOR_FETCHING_URL_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_BNPL_PAYMENT_INSTRUMENT_FOR_FETCHING_URL_REQUEST_H_

#include "base/functional/callback.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

class GetBnplPaymentInstrumentForFetchingUrlRequest : public PaymentsRequest {
 public:
  GetBnplPaymentInstrumentForFetchingUrlRequest(
      GetBnplPaymentInstrumentForFetchingUrlRequestDetails request_details,
      bool full_sync_enabled,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const BnplFetchUrlResponseDetails&)> callback);
  GetBnplPaymentInstrumentForFetchingUrlRequest(
      const GetBnplPaymentInstrumentForFetchingUrlRequest&) = delete;
  GetBnplPaymentInstrumentForFetchingUrlRequest& operator=(
      const GetBnplPaymentInstrumentForFetchingUrlRequest&) = delete;
  ~GetBnplPaymentInstrumentForFetchingUrlRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  GetBnplPaymentInstrumentForFetchingUrlRequestDetails request_details_;
  bool full_sync_enabled_;
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                          const BnplFetchUrlResponseDetails&)>
      callback_;
  BnplFetchUrlResponseDetails response_details_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_GET_BNPL_PAYMENT_INSTRUMENT_FOR_FETCHING_URL_REQUEST_H_
