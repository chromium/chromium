// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENTS_FACILITATED_PAYMENTS_INITIATE_PAYMENT_REQUEST_DETAILS_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENTS_FACILITATED_PAYMENTS_INITIATE_PAYMENT_REQUEST_DETAILS_H_

namespace payments::facilitated {

// Contains information to make a `FacilitatedPaymentsInitiatePaymentRequest`.
class FacilitatedPaymentsInitiatePaymentRequestDetails {
 public:
  FacilitatedPaymentsInitiatePaymentRequestDetails();
  FacilitatedPaymentsInitiatePaymentRequestDetails(
      const FacilitatedPaymentsInitiatePaymentRequestDetails&) = delete;
  FacilitatedPaymentsInitiatePaymentRequestDetails& operator=(
      const FacilitatedPaymentsInitiatePaymentRequestDetails&) = delete;
  ~FacilitatedPaymentsInitiatePaymentRequestDetails() = default;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PAYMENTS_FACILITATED_PAYMENTS_INITIATE_PAYMENT_REQUEST_DETAILS_H_
