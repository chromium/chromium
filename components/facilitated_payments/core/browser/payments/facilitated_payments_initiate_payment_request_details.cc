// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/payments/facilitated_payments_initiate_payment_request_details.h"

namespace payments::facilitated {

FacilitatedPaymentsInitiatePaymentRequestDetails::
    FacilitatedPaymentsInitiatePaymentRequestDetails() = default;

FacilitatedPaymentsInitiatePaymentRequestDetails::
    ~FacilitatedPaymentsInitiatePaymentRequestDetails() = default;

void FacilitatedPaymentsInitiatePaymentRequestDetails::Reset() {
  risk_data_.clear();
  client_token_.clear();
  instrument_id_.reset();
  pix_code_.reset();
}

bool FacilitatedPaymentsInitiatePaymentRequestDetails::IsReadyForPixPayment() {
  return instrument_id_.has_value() && pix_code_.has_value() &&
         !risk_data_.empty() && !client_token_.empty();
}

}  // namespace payments::facilitated
