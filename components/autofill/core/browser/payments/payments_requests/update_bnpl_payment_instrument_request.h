// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPDATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPDATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace autofill::payments {

class UpdateBnplPaymentInstrumentRequest : public PaymentsRequest {
 public:
  UpdateBnplPaymentInstrumentRequest(
      UpdateBnplPaymentInstrumentRequestDetails request_details,
      bool full_sync_enabled,
      base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)>
          callback);
  UpdateBnplPaymentInstrumentRequest(
      const UpdateBnplPaymentInstrumentRequest&) = delete;
  UpdateBnplPaymentInstrumentRequest& operator=(
      const UpdateBnplPaymentInstrumentRequest&) = delete;
  ~UpdateBnplPaymentInstrumentRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  friend class UpdateBnplPaymentInstrumentRequestTestApi;

  UpdateBnplPaymentInstrumentRequestDetails request_details_;
  bool full_sync_enabled_;
  bool received_buy_now_pay_later_info_ = false;
  base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult)> callback_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_UPDATE_BNPL_PAYMENT_INSTRUMENT_REQUEST_H_
