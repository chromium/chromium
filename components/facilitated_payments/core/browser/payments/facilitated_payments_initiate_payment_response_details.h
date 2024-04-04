// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENTS_FACILITATED_PAYMENTS_INITIATE_PAYMENT_RESPONSE_DETAILS_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENTS_FACILITATED_PAYMENTS_INITIATE_PAYMENT_RESPONSE_DETAILS_H_

namespace payments::facilitated {

// Contains information retrieved from an
// `FacilitatedPaymentsInitiatePaymentRequest`.
class FacilitatedPaymentsInitiatePaymentResponseDetails {
 public:
  FacilitatedPaymentsInitiatePaymentResponseDetails();
  FacilitatedPaymentsInitiatePaymentResponseDetails(
      const FacilitatedPaymentsInitiatePaymentResponseDetails&) = delete;
  FacilitatedPaymentsInitiatePaymentResponseDetails& operator=(
      const FacilitatedPaymentsInitiatePaymentResponseDetails&) = delete;
  ~FacilitatedPaymentsInitiatePaymentResponseDetails() = default;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENTS_FACILITATED_PAYMENTS_INITIATE_PAYMENT_RESPONSE_DETAILS_H_
