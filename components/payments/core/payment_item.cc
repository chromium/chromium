// Copyright 2017 The Chromium Authors. All rights reserved.
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

bool PaymentItem::FromValue(const base::Value& value) {
  if (!value.is_dict())
    return false;

  const std::string* label_val = value.FindStringKey(kPaymentItemLabel);
  if (!label_val) {
    return false;
  }
  label = *label_val;

  const base::Value* amount_dict = value.FindDictKey(kPaymentItemAmount);
  if (!amount_dict) {
    return false;
  }
  amount = mojom::PaymentCurrencyAmount::New();
  if (!PaymentCurrencyAmountFromValue(*amount_dict, amount.get())) {
    return false;
  }

  // Pending is optional.
  pending = value.FindBoolKey(kPaymentItemPending).value_or(pending);

  return true;
}

base::Value PaymentItem::ToValue() const {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetStringKey(kPaymentItemLabel, label);
  result.SetKey(kPaymentItemAmount, PaymentCurrencyAmountToValue(*amount));
  result.SetBoolKey(kPaymentItemPending, pending);

  return result;
}

}  // namespace payments
