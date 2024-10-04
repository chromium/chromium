// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_OPTIONS_PROVIDER_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_OPTIONS_PROVIDER_H_

#include <stdint.h>

namespace payments {

// See PaymentOptionsProvider::shipping_type() below.
enum class PaymentShippingType : int32_t {
  SHIPPING = 0,
  DELIVERY = 1,
  PICKUP = 2,
};

// An interface which provides immutable values, specified by the merchant at
// request-time, describing the set of information required from the payer, and
// possibly the method by which the order will be fulfilled.
class PaymentOptionsProvider {
 public:
  virtual ~PaymentOptionsProvider() = default;

  // Returns true if this transaction requires the payer's name.
  virtual bool request_payer_name() const = 0;

  // Returns true if this transaction requires the payer's email address.
  virtual bool request_payer_email() const = 0;

  // Returns true if this transaction requires the payer's phone number.
  virtual bool request_payer_phone() const = 0;

  // Returns true if this transaction requires a shipping address.
  virtual bool request_shipping() const = 0;

  // A value, provided by the merchant at request-time, indicating the method
  // by which the order will be fulfilled. Used only to modify presentation of
  // the user interface, and only meaningful when request_shipping() is true.
  virtual PaymentShippingType shipping_type() const = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_OPTIONS_PROVIDER_H_
