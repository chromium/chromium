// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_ADDRESS_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_ADDRESS_H_

#include "base/values.h"
#include "components/payments/mojom/payment_request_data.mojom.h"

// C++ bindings for the PaymentRequest API PaymentAddress. Conforms to the
// following spec:
// https://w3c.github.io/payment-request/#dom-paymentaddress

namespace payments {

// Returns a base::DictValue with the properties of this PaymentAddress.
base::DictValue PaymentAddressToValueDict(const mojom::PaymentAddress& address);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_ADDRESS_H_
