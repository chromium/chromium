// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_details.h"

#include <algorithm>

#include "base/memory/values_equivalent.h"
#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/payment-request/#payment-details-dictionaries
static const char kPaymentDetailsAdditionalDisplayItems[] =
    "additionalDisplayItems";
static const char kPaymentDetailsDisplayItems[] = "displayItems";
static const char kPaymentDetailsError[] = "error";
static const char kPaymentDetailsId[] = "id";
static const char kPaymentDetailsModifiers[] = "modifiers";
static const char kPaymentDetailsShippingOptions[] = "shippingOptions";
static const char kPaymentDetailsTotal[] = "total";

}  // namespace

PaymentDetails::PaymentDetails() {}
PaymentDetails::~PaymentDetails() = default;

PaymentDetails::PaymentDetails(const PaymentDetails& other) {
  *this = other;
}

PaymentDetails& PaymentDetails::operator=(const PaymentDetails& other) {
  id = other.id;
  if (other.total)
    total = std::make_unique<PaymentItem>(*other.total);
  else
    total.reset(nullptr);

  display_items.clear();
  display_items.reserve(other.display_items.size());
  for (const auto& item : other.display_items) {
    display_items.push_back(item);
  }

  shipping_options = std::vector<PaymentShippingOption>(other.shipping_options);
  modifiers = std::vector<PaymentDetailsModifier>(other.modifiers);
  return *this;
}

bool PaymentDetails::operator==(const PaymentDetails& other) const {
  return id == other.id && base::ValuesEquivalent(total, other.total) &&
         display_items == other.display_items &&
         shipping_options == other.shipping_options &&
         modifiers == other.modifiers && error == other.error;
}

bool PaymentDetails::operator!=(const PaymentDetails& other) const {
  return !(*this == other);
}

bool PaymentDetails::FromValue(const base::Value& value, bool requires_total) {
  display_items.clear();
  shipping_options.clear();
  modifiers.clear();

  if (!value.is_dict()) {
    return false;
  }

  // ID is optional.
  const std::string* specified_id = value.FindStringKey(kPaymentDetailsId);
  if (specified_id)
    id = *specified_id;

  const base::Value* total_dict = value.FindDictKey(kPaymentDetailsTotal);
  if (!total_dict && requires_total) {
    return false;
  }

  if (total_dict) {
    total = std::make_unique<PaymentItem>();
    if (!total->FromValue(*total_dict)) {
      return false;
    }
  }

  const base::Value* display_items_list =
      value.FindListKey(kPaymentDetailsDisplayItems);
  if (display_items_list) {
    for (const base::Value& payment_item_dict :
         display_items_list->GetListDeprecated()) {
      PaymentItem payment_item;
      if (!payment_item.FromValue(payment_item_dict)) {
        return false;
      }
      display_items.push_back(payment_item);
    }
  }

  const base::Value* shipping_options_list =
      value.FindListKey(kPaymentDetailsShippingOptions);
  if (shipping_options_list) {
    for (const base::Value& shipping_option_dict :
         shipping_options_list->GetListDeprecated()) {
      PaymentShippingOption shipping_option;
      if (!shipping_option.FromValue(shipping_option_dict)) {
        return false;
      }
      shipping_options.push_back(shipping_option);
    }
  }

  const base::Value* modifiers_list =
      value.FindListKey(kPaymentDetailsModifiers);
  if (modifiers_list) {
    for (const base::Value& modifier_dict :
         modifiers_list->GetListDeprecated()) {
      PaymentDetailsModifier modifier;
      if (!modifier.method_data.FromValue(modifier_dict)) {
        return false;
      }
      const base::Value* modifier_total_dict =
          modifier_dict.FindDictKey(kPaymentDetailsTotal);
      if (modifier_total_dict) {
        modifier.total = std::make_unique<PaymentItem>();
        if (!modifier.total->FromValue(*modifier_total_dict))
          return false;
      }
      const base::Value* additional_display_items_list =
          modifier_dict.FindListKey(kPaymentDetailsAdditionalDisplayItems);
      if (additional_display_items_list) {
        for (const base::Value& additional_display_item_dict :
             additional_display_items_list->GetListDeprecated()) {
          PaymentItem additional_display_item;
          if (!additional_display_item.FromValue(
                  additional_display_item_dict)) {
            return false;
          }
          modifier.additional_display_items.push_back(additional_display_item);
        }
      }
      modifiers.push_back(modifier);
    }
  }

  // Error is optional.
  const std::string* specified_error =
      value.FindStringKey(kPaymentDetailsError);
  if (specified_error)
    error = *specified_error;

  return true;
}

}  // namespace payments
