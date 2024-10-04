// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_details_modifier.h"

#include "base/memory/values_equivalent.h"
#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/payment-request/#dom-paymentdetailsmodifier
static const char kPaymentDetailsModifierTotal[] = "total";
static const char kPaymentDetailsModifierSupportedMethods[] =
    "supportedMethods";
static const char kPaymentDetailsModifierData[] = "data";

}  // namespace

PaymentDetailsModifier::PaymentDetailsModifier() = default;
PaymentDetailsModifier::~PaymentDetailsModifier() = default;

PaymentDetailsModifier::PaymentDetailsModifier(
    const PaymentDetailsModifier& other) {
  *this = other;
}

PaymentDetailsModifier& PaymentDetailsModifier::operator=(
    const PaymentDetailsModifier& other) {
  method_data = other.method_data;
  if (other.total) {
    total = std::make_unique<PaymentItem>(*other.total);
  } else {
    total.reset(nullptr);
  }
  additional_display_items = std::vector<PaymentItem>();
  for (const auto& item : other.additional_display_items)
    additional_display_items.emplace_back(item);
  return *this;
}

bool PaymentDetailsModifier::operator==(
    const PaymentDetailsModifier& other) const {
  return method_data == other.method_data &&
         base::ValuesEquivalent(total, other.total) &&
         additional_display_items == other.additional_display_items;
}

bool PaymentDetailsModifier::operator!=(
    const PaymentDetailsModifier& other) const {
  return !(*this == other);
}

base::Value::Dict PaymentDetailsModifier::ToValueDict() const {
  base::Value::Dict result;
  result.Set(kPaymentDetailsModifierSupportedMethods,
             method_data.supported_method);
  result.Set(kPaymentDetailsModifierData, method_data.data);
  if (total) {
    result.Set(kPaymentDetailsModifierTotal, total->ToValueDict());
  }

  return result;
}

}  // namespace payments
