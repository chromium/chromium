// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_SHIPPING_OPTION_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_SHIPPING_OPTION_H_

#include <string>

#include "components/payments/core/payment_currency_amount.h"

// C++ bindings for the PaymentRequest API PaymentShippingOption. Conforms to
// the following spec:
// https://w3c.github.io/browser-payment-api/#dom-paymentshippingoption

namespace base {
class Value;
}

namespace payments {

// Information describing a shipping option.
class PaymentShippingOption {
 public:
  PaymentShippingOption();
  PaymentShippingOption(const PaymentShippingOption& other);
  ~PaymentShippingOption();

  bool operator==(const PaymentShippingOption& other) const;
  bool operator!=(const PaymentShippingOption& other) const;
  PaymentShippingOption& operator=(const PaymentShippingOption& other);

  // Populates the properties of this PaymentShippingOption from |dict|.
  // Returns true if the required values are present.
  bool FromValueDict(const base::Value::Dict& dict);

  // An identifier used to reference this PaymentShippingOption. It is unique
  // for a given PaymentRequest.
  std::string id;

  // A human-readable description of the item. The user agent should use this
  // string to display the shipping option to the user.
  std::string label;

  // A PaymentCurrencyAmount containing the monetary amount for the option.
  mojom::PaymentCurrencyAmountPtr amount;

  // This is set to true to indicate that this is the default selected
  // PaymentShippingOption in a sequence. User agents should display this option
  // by default in the user interface.
  bool selected;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_SHIPPING_OPTION_H_
