// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_item.h"

#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/payment-request/#dom-paymentitem
static const char kPaymentItemAmount[] = "amount";
static const char kPaymentItemLabel[] = "label";
static const char kPaymentItemPending[] = "pending";

}  // namespace

PaymentItem::PaymentItem()
    : amount(mojom::PaymentCurrencyAmount::New()), pending(false) {}

PaymentItem::~PaymentItem() = default;

PaymentItem::PaymentItem(const PaymentItem& other) {
  *this = other;
}

bool PaymentItem::operator==(const PaymentItem& other) const {
  return label == other.label && amount.Equals(other.amount) &&
         pending == other.pending;
}

bool PaymentItem::operator!=(const PaymentItem& other) const {
  return !(*this == other);
}

PaymentItem& PaymentItem::operator=(const PaymentItem& other) {
  label = other.label;
  if (other.amount) {
    amount = other.amount->Clone();
  } else {
    amount.reset();
  }
  pending = other.pending;
  return *this;
}

bool PaymentItem::FromValueDict(const base::Value::Dict& dict) {
  const std::string* label_val = dict.FindString(kPaymentItemLabel);
  if (!label_val) {
    return false;
  }
  label = *label_val;

  const base::Value::Dict* amount_dict = dict.FindDict(kPaymentItemAmount);
  if (!amount_dict) {
    return false;
  }
  amount = mojom::PaymentCurrencyAmount::New();
  if (!PaymentCurrencyAmountFromValueDict(*amount_dict, amount.get())) {
    return false;
  }

  // Pending is optional.
  pending = dict.FindBool(kPaymentItemPending).value_or(pending);

  return true;
}

base::Value::Dict PaymentItem::ToValueDict() const {
  base::Value::Dict result;
  result.Set(kPaymentItemLabel, label);
  result.Set(kPaymentItemAmount, PaymentCurrencyAmountToValueDict(*amount));
  result.Set(kPaymentItemPending, pending);

  return result;
}

}  // namespace payments
