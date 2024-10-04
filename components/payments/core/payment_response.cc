// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_response.h"

#include "components/payments/core/payment_address.h"

namespace payments {

PaymentResponse::PaymentResponse() = default;
PaymentResponse::~PaymentResponse() = default;

bool PaymentResponse::operator==(const PaymentResponse& other) const {
  return payment_request_id == other.payment_request_id &&
         method_name == other.method_name && details == other.details &&
         shipping_address.Equals(other.shipping_address) &&
         shipping_option == other.shipping_option &&
         payer_name == other.payer_name && payer_email == other.payer_email &&
         payer_phone == other.payer_phone;
}

bool PaymentResponse::operator!=(const PaymentResponse& other) const {
  return !(*this == other);
}

}  // namespace payments
