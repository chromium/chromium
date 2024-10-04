// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_details.h"

#include <algorithm>

#include "base/memory/values_equivalent.h"

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

PaymentDetails::PaymentDetails() = default;
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

bool PaymentDetails::FromValueDict(const base::Value::Dict& dict,
                                   bool requires_total) {
  display_items.clear();
  shipping_options.clear();
  modifiers.clear();

  // ID is optional.
  const std::string* specified_id = dict.FindString(kPaymentDetailsId);
  if (specified_id)
    id = *specified_id;

  const base::Value::Dict* total_dict = dict.FindDict(kPaymentDetailsTotal);
  if (!total_dict && requires_total) {
    return false;
  }

  if (total_dict) {
    total = std::make_unique<PaymentItem>();
    if (!total->FromValueDict(*total_dict)) {
      return false;
    }
  }

  const base::Value::List* display_items_list =
      dict.FindList(kPaymentDetailsDisplayItems);
  if (display_items_list) {
    for (const base::Value& payment_item_list_entry : *display_items_list) {
      PaymentItem payment_item;
      if (!payment_item_list_entry.is_dict() ||
          !payment_item.FromValueDict(payment_item_list_entry.GetDict())) {
        return false;
      }
      display_items.push_back(payment_item);
    }
  }

  const base::Value::List* shipping_options_list =
      dict.FindList(kPaymentDetailsShippingOptions);
  if (shipping_options_list) {
    for (const base::Value& shipping_option_list_entry :
         *shipping_options_list) {
      PaymentShippingOption shipping_option;
      if (!shipping_option_list_entry.is_dict() ||
          !shipping_option.FromValueDict(
              shipping_option_list_entry.GetDict())) {
        return false;
      }
      shipping_options.push_back(shipping_option);
    }
  }

  const base::Value::List* modifiers_list =
      dict.FindList(kPaymentDetailsModifiers);
  if (modifiers_list) {
    for (const base::Value& modifiers_list_element : *modifiers_list) {
      if (!modifiers_list_element.is_dict())
        return false;
      const base::Value::Dict& modifier_dict = modifiers_list_element.GetDict();
      PaymentDetailsModifier modifier;
      if (!modifier.method_data.FromValueDict(modifier_dict)) {
        return false;
      }
      const base::Value::Dict* modifier_total_dict =
          modifier_dict.FindDict(kPaymentDetailsTotal);
      if (modifier_total_dict) {
        modifier.total = std::make_unique<PaymentItem>();
        if (!modifier.total->FromValueDict(*modifier_total_dict))
          return false;
      }
      const base::Value::List* additional_display_items_list =
          modifier_dict.FindList(kPaymentDetailsAdditionalDisplayItems);
      if (additional_display_items_list) {
        for (const base::Value& additional_display_items_list_elem :
             *additional_display_items_list) {
          if (!additional_display_items_list_elem.is_dict())
            return false;
          PaymentItem additional_display_item;
          if (!additional_display_item.FromValueDict(
                  additional_display_items_list_elem.GetDict())) {
            return false;
          }
          modifier.additional_display_items.push_back(additional_display_item);
        }
      }
      modifiers.push_back(modifier);
    }
  }

  // Error is optional.
  const std::string* specified_error = dict.FindString(kPaymentDetailsError);
  if (specified_error)
    error = *specified_error;

  return true;
}

}  // namespace payments
