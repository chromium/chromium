// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_RESPONSE_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_RESPONSE_H_

#include <memory>
#include <string>

#include "components/payments/mojom/payment_request_data.mojom.h"

// C++ bindings for the PaymentRequest API PaymentResponse. Conforms to the
// following spec:
// https://www.w3.org/TR/payment-request/#paymentresponse-interface

namespace payments {

// Information provided in the Promise returned by a call to
// PaymentRequest.show().
class PaymentResponse {
 public:
  PaymentResponse();
  ~PaymentResponse();

  bool operator==(const PaymentResponse& other) const;
  bool operator!=(const PaymentResponse& other) const;

  // The same ID present in the original PaymentRequest.
  std::string payment_request_id;

  // The payment method identifier for the payment method that the user selected
  // to fulfil the transaction.
  std::string method_name;

  // The json-serialized stringified details of the payment method. Used by the
  // merchant to process the transaction and determine successful fund transfer.
  std::string details;

  // If request_shipping was set to true in the PaymentOptions passed to the
  // PaymentRequest constructor, this will be the full and final shipping
  // address chosen by the user.
  mojom::PaymentAddressPtr shipping_address;

  // If the request_shipping flag was set to true in the PaymentOptions passed
  // to the PaymentRequest constructor, this will be the id attribute of the
  // selected shipping option.
  std::string shipping_option;

  // If the request_payer_name flag was set to true in the PaymentOptions passed
  // to the PaymentRequest constructor, this will be the name provided by the
  // user.
  std::u16string payer_name;

  // If the request_payer_email flag was set to true in the PaymentOptions
  // passed to the PaymentRequest constructor, this will be the email address
  // chosen by the user.
  std::u16string payer_email;

  // If the request_payer_phone flag was set to true in the PaymentOptions
  // passed to the PaymentRequest constructor, this will be the phone number
  // chosen by the user.
  std::u16string payer_phone;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_RESPONSE_H_
