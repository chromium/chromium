// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/payments/facilitated_payments_initiate_payment_request.h"

namespace payments::facilitated {

FacilitatedPaymentsInitiatePaymentRequest::
    FacilitatedPaymentsInitiatePaymentRequest(
        std::unique_ptr<FacilitatedPaymentsInitiatePaymentRequestDetails>
            request_details,
        FacilitatedPaymentsNetworkInterface::InitiatePaymentResponseCallback
            response_callback)
    : request_details_(std::move(request_details)),
      response_callback_(std::move(response_callback)) {}

FacilitatedPaymentsInitiatePaymentRequest::
    ~FacilitatedPaymentsInitiatePaymentRequest() = default;

std::string FacilitatedPaymentsInitiatePaymentRequest::GetRequestUrlPath() {
  return "";
}

std::string FacilitatedPaymentsInitiatePaymentRequest::GetRequestContentType() {
  return "";
}

std::string FacilitatedPaymentsInitiatePaymentRequest::GetRequestContent() {
  return "";
}

void FacilitatedPaymentsInitiatePaymentRequest::ParseResponse(
    const base::Value::Dict& response) {}

bool FacilitatedPaymentsInitiatePaymentRequest::IsResponseComplete() {
  return false;
}

void FacilitatedPaymentsInitiatePaymentRequest::RespondToDelegate(
    autofill::AutofillClient::PaymentsRpcResult result) {}

}  // namespace payments::facilitated
