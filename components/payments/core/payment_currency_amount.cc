// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_currency_amount.h"

#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/browser-payment-api/#dom-paymentcurrencyamount
static const char kPaymentCurrencyAmountCurrency[] = "currency";
static const char kPaymentCurrencyAmountValue[] = "value";

}  // namespace

bool PaymentCurrencyAmountFromValueDict(
    const base::Value::Dict& dictionary_value,
    mojom::PaymentCurrencyAmount* amount) {
  const std::string* currency =
      dictionary_value.FindString(kPaymentCurrencyAmountCurrency);
  if (!currency) {
    return false;
  }
  amount->currency = *currency;

  const std::string* value =
      dictionary_value.FindString(kPaymentCurrencyAmountValue);
  if (!value) {
    return false;
  }
  amount->value = *value;

  return true;
}

base::Value::Dict PaymentCurrencyAmountToValueDict(
    const mojom::PaymentCurrencyAmount& amount) {
  base::Value::Dict result;
  result.Set(kPaymentCurrencyAmountCurrency, amount.currency);
  result.Set(kPaymentCurrencyAmountValue, amount.value);

  return result;
}

}  // namespace payments
