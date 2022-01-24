// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_BASIC_CARD_RESPONSE_H_
#define COMPONENTS_PAYMENTS_CORE_BASIC_CARD_RESPONSE_H_

#include <memory>
#include <string>

#include "components/payments/core/payment_address.h"

namespace base {
class Value;
}

namespace payments {

// Contains the response from the PaymentRequest API when a user accepts
// payment with a Basic Payment Card payment method.
struct BasicCardResponse {
 public:
  BasicCardResponse();
  ~BasicCardResponse();

  bool operator==(const BasicCardResponse& other) const;
  bool operator!=(const BasicCardResponse& other) const;

  // Populates |value| with the properties of this BasicCardResponse.
  base::Value ToValue() const;

  // The cardholder's name as it appears on the card.
  std::u16string cardholder_name;

  // The primary account number (PAN) for the payment card.
  std::u16string card_number;

  // A two-digit string for the expiry month of the card in the range 01 to 12.
  std::u16string expiry_month;

  // A two-digit string for the expiry year of the card in the range 00 to 99.
  std::u16string expiry_year;

  // A three or four digit string for the security code of the card (sometimes
  // known as the CVV, CVC, CVN, CVE or CID).
  std::u16string card_security_code;

  // The billing address information associated with the payment card.
  mojom::PaymentAddressPtr billing_address;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_BASIC_CARD_RESPONSE_H_
