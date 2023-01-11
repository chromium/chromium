// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_DETAILS_CONVERTER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_DETAILS_CONVERTER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "third_party/blink/public/mojom/payments/payment_handler_host.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {

class PaymentDetailsConverter {
 public:
  using MethodChecker =
      base::RepeatingCallback<void(const std::string& payment_method_identifier,
                                   bool* is_valid)>;

  PaymentDetailsConverter() = delete;
  PaymentDetailsConverter(const PaymentDetailsConverter&) = delete;
  PaymentDetailsConverter& operator=(const PaymentDetailsConverter&) = delete;

  // Converts and redacts the |details| from the merchant's updateWith(details)
  // call into a data structure that can be sent to the payment handler.
  //
  // The |details| should not be null.
  // Shipping related information is redacted when |handles_shipping| is false.
  // The |method_checker| is not saved. It is used only for the duration of this
  // call.
  static mojom::PaymentRequestDetailsUpdatePtr
  ConvertToPaymentRequestDetailsUpdate(const mojom::PaymentDetailsPtr& details,
                                       bool handles_shipping,
                                       const MethodChecker& method_checker);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_DETAILS_CONVERTER_H_
