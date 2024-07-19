// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_FACILITATED_PAYMENTS_INITIATE_PAYMENT_REQUEST_DETAILS_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_FACILITATED_PAYMENTS_INITIATE_PAYMENT_REQUEST_DETAILS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "url/gurl.h"

namespace payments::facilitated {

// Contains information to make a `FacilitatedPaymentsInitiatePaymentRequest`.
class FacilitatedPaymentsInitiatePaymentRequestDetails {
 public:
  FacilitatedPaymentsInitiatePaymentRequestDetails();
  FacilitatedPaymentsInitiatePaymentRequestDetails(
      const FacilitatedPaymentsInitiatePaymentRequestDetails&) = delete;
  FacilitatedPaymentsInitiatePaymentRequestDetails& operator=(
      const FacilitatedPaymentsInitiatePaymentRequestDetails&) = delete;
  ~FacilitatedPaymentsInitiatePaymentRequestDetails();

  // Returns true if everything that's required for making a PIX payment request
  // is available.
  bool IsReadyForPixPayment();

  std::string risk_data_;
  std::vector<uint8_t> client_token_;
  std::optional<int64_t> billing_customer_number_;
  std::optional<std::string> merchant_payment_page_hostname_;
  // The identifier for the payment method selected by the user. This is a
  // required field. Its type is optional to avoid its default value being 0.
  std::optional<int64_t> instrument_id_;
  std::optional<std::string> pix_code_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_FACILITATED_PAYMENTS_INITIATE_PAYMENT_REQUEST_DETAILS_H_
