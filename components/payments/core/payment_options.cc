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

}  // namespace payments
