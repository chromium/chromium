// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_CURRENCY_AMOUNT_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_CURRENCY_AMOUNT_H_

#include "base/values.h"
#include "components/payments/mojom/payment_request_data.mojom.h"

// C++ bindings for the PaymentRequest API PaymentCurrencyAmount. Conforms to
// the following spec:
// https://w3c.github.io/browser-payment-api/#dom-paymentcurrencyamount

namespace base {
class Value;
}

namespace payments {

// Populates the properties of |amount| from |dictionary_value|.
// Returns true if the required values are present.
bool PaymentCurrencyAmountFromValueDict(
    const base::Value::Dict& dictionary_value,
    mojom::PaymentCurrencyAmount* amount);

// Creates a base::Value::Dict with the properties of the given
// PaymentCurrencyAmount.
base::Value::Dict PaymentCurrencyAmountToValueDict(
    const mojom::PaymentCurrencyAmount& amount);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_CURRENCY_AMOUNT_H_
