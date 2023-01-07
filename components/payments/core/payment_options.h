// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_OPTIONS_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_OPTIONS_H_

#include "components/payments/core/payment_options_provider.h"

// C++ bindings for the PaymentRequest API PaymentOptions. Conforms to the
// following spec:
// https://w3c.github.io/payment-request/#paymentoptions-dictionary

namespace payments {

class PaymentOptions {
 public:
  PaymentOptions();
  ~PaymentOptions();

  bool operator==(const PaymentOptions& other) const;
  bool operator!=(const PaymentOptions& other) const;

  // Indicates whether the user agent should collect and return the payer's name
  // as part of the payment request. For example, this would be set to true to
  // allow a merchant to make a booking in the payer's name.
  bool request_payer_name;

  // Indicates whether the user agent should collect and return the payer's
  // email address as part of the payment request. For example, this would be
  // set to true to allow a merchant to email a receipt.
  bool request_payer_email;

  // Indicates whether the user agent should collect and return the payer's
  // phone number as part of the payment request. For example, this would be set
  // to true to allow a merchant to phone a customer with a billing enquiry.
  bool request_payer_phone;

  // Indicates whether the user agent should collect and return a shipping
  // address as part of the payment request. For example, this would be set to
  // true when physical goods need to be shipped by the merchant to the user.
  // This would be set to false for an online-only electronic purchase
  // transaction.
  bool request_shipping;

  // If request_shipping is set to true, then this field may only be used to
  // influence the way the user agent presents the user interface for gathering
  // the shipping address.
  PaymentShippingType shipping_type;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_OPTIONS_H_
