// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_FACILITATED_PAYMENTS_INITIATE_PAYMENT_RESPONSE_DETAILS_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_FACILITATED_PAYMENTS_INITIATE_PAYMENT_RESPONSE_DETAILS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "components/facilitated_payments/core/browser/model/secure_payload.h"

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
  ~FacilitatedPaymentsInitiatePaymentResponseDetails();

  // Used to trigger a payment UI flow in Google Play Services.
  SecurePayload secure_payload_;

  // Set if the request to Payments API fails. The message is human-readable,
  // and will be shown to the user. It could contain HTML <a> tags linking to
  // help center docs.
  std::optional<std::string> error_message_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NETWORK_API_FACILITATED_PAYMENTS_INITIATE_PAYMENT_RESPONSE_DETAILS_H_
