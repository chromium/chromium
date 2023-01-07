// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_options.h"

#include "base/values.h"

namespace payments {

PaymentOptions::PaymentOptions()
    : request_payer_name(false),
      request_payer_email(false),
      request_payer_phone(false),
      request_shipping(false),
      shipping_type(payments::PaymentShippingType::SHIPPING) {}
PaymentOptions::~PaymentOptions() = default;

bool PaymentOptions::operator==(const PaymentOptions& other) const {
  return request_payer_name == other.request_payer_name &&
         request_payer_email == other.request_payer_email &&
         request_payer_phone == other.request_payer_phone &&
         request_shipping == other.request_shipping &&
         shipping_type == other.shipping_type;
}

bool PaymentOptions::operator!=(const PaymentOptions& other) const {
  return !(*this == other);
}

}  // namespace payments
