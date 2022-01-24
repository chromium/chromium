// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_CURRENCY_AMOUNT_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_CURRENCY_AMOUNT_H_

#include "components/payments/mojom/payment_request_data.mojom.h"

// C++ bindings for the PaymentRequest API PaymentCurrencyAmount. Conforms to
// the following spec:
// https://w3c.github.io/browser-payment-api/#dom-paymentcurrencyamount

namespace base {
class Value;
}

namespace payments {

// Populates the properties of |amount| from |value|. Returns true if the
// required values are present.
bool PaymentCurrencyAmountFromValue(const base::Value& dictionary_value,
                                    mojom::PaymentCurrencyAmount* amount);

// Creates a base::Value dictionary with the properties of the given
// PaymentCurrencyAmount.
base::Value PaymentCurrencyAmountToValue(
    const mojom::PaymentCurrencyAmount& amount);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_CURRENCY_AMOUNT_H_
