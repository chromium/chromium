// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_EVENT_RESPONSE_UTIL_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_EVENT_RESPONSE_UTIL_H_

#include <string_view>

#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"

namespace payments {

// Converts the given 'canmakepayment' event |response_type| into a
// developer-facing error string. SUCCESS is converted into an empty string.
std::string_view ConvertCanMakePaymentEventResponseTypeToErrorString(
    mojom::CanMakePaymentEventResponseType response_type);

// Converts the given 'paymentrequest' event |response_type| into a
// developer-facing error string. PAYMENT_EVENT_SUCCESS is converted into an
// empty string.
std::string_view ConvertPaymentEventResponseTypeToErrorString(
    mojom::PaymentEventResponseType response_type);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_EVENT_RESPONSE_UTIL_H_
