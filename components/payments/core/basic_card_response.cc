// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/basic_card_response.h"

#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/webpayments-methods-card/#basiccardresponse
static const char kCardBillingAddress[] = "billingAddress";
static const char kCardCardholderName[] = "cardholderName";
static const char kCardCardNumber[] = "cardNumber";
static const char kCardCardSecurityCode[] = "cardSecurityCode";
static const char kCardExpiryMonth[] = "expiryMonth";
static const char kCardExpiryYear[] = "expiryYear";

}  // namespace

BasicCardResponse::BasicCardResponse()
    : billing_address(mojom::PaymentAddress::New()) {}

BasicCardResponse::~BasicCardResponse() = default;

bool BasicCardResponse::operator==(const BasicCardResponse& other) const {
  return cardholder_name == other.cardholder_name &&
         card_number == other.card_number &&
         expiry_month == other.expiry_month &&
         expiry_year == other.expiry_year &&
         card_security_code == other.card_security_code &&
         billing_address.Equals(other.billing_address);
}

bool BasicCardResponse::operator!=(const BasicCardResponse& other) const {
  return !(*this == other);
}

base::Value BasicCardResponse::ToValue() const {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetStringKey(kCardCardholderName, cardholder_name);
  result.SetStringKey(kCardCardNumber, card_number);
  result.SetStringKey(kCardExpiryMonth, expiry_month);
  result.SetStringKey(kCardExpiryYear, expiry_year);
  result.SetStringKey(kCardCardSecurityCode, card_security_code);
  result.SetKey(kCardBillingAddress, PaymentAddressToValue(*billing_address));

  return result;
}

}  // namespace payments
