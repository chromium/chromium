// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_initiate_payment_request_details.h"

namespace payments::facilitated {

FacilitatedPaymentsInitiatePaymentRequestDetails::
    FacilitatedPaymentsInitiatePaymentRequestDetails() = default;

FacilitatedPaymentsInitiatePaymentRequestDetails::
    ~FacilitatedPaymentsInitiatePaymentRequestDetails() = default;

bool FacilitatedPaymentsInitiatePaymentRequestDetails::IsReadyForPixPayment() {
  return !risk_data_.empty() && !client_token_.empty() &&
         billing_customer_number_.has_value() &&
         merchant_payment_page_hostname_.has_value() &&
         instrument_id_.has_value() && pix_code_.has_value();
}

}  // namespace payments::facilitated
