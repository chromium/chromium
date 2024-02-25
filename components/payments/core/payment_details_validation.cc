// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_details_validation.h"

#include <set>
#include <vector>

#include "components/payments/core/payment_details.h"
#include "components/payments/core/payments_validators.h"

namespace payments {
namespace {

// Validates ShippingOption or PaymentItem, which happen to have identical
// fields, except for "id", which is present only in ShippingOption.
template <typename T>
bool ValidateShippingOptionOrPaymentItem(const T& item,
                                         std::string* error_message) {
  if (!item.amount) {
    *error_message = "Amount required";
    return false;
  }

  if (item.amount->currency.empty()) {
    *error_message = "Currency code required";
    return false;
  }

  if (item.amount->value.empty()) {
    *error_message = "Currency value required";
    return false;
  }

  if (!payments::PaymentsValidators::IsValidCurrencyCodeFormat(
          item.amount->currency, error_message)) {
    return false;
  }

  if (!payments::PaymentsValidators::IsValidAmountFormat(item.amount->value,
                                                         error_message)) {
    return false;
  }
  return true;
}

bool ValidateDisplayItems(const std::vector<PaymentItem>& items,
                          std::string* error_message) {
  for (const auto& item : items) {
    if (!ValidateShippingOptionOrPaymentItem(item, error_message)) {
      return false;
    }
  }
  return true;
}

bool ValidateShippingOptions(const std::vector<PaymentShippingOption>& options,
                             std::string* error_message) {
  std::set<std::string> uniqueIds;
  for (const auto& option : options) {
    if (option.id.empty()) {
      *error_message = "ShippingOption id required";
      return false;
    }

    if (uniqueIds.find(option.id) != uniqueIds.end()) {
      *error_message = "Duplicate shipping option identifiers are not allowed";
      return false;
    }
    uniqueIds.insert(option.id);

    if (!ValidateShippingOptionOrPaymentItem(option, error_message)) {
      return false;
    }
  }
  return true;
}

bool ValidatePaymentDetailsModifiers(
    const std::vector<PaymentDetailsModifier>& modifiers,
    std::string* error_message) {
  if (modifiers.empty()) {
    *error_message = "Must specify at least one payment details modifier";
    return false;
  }

  for (const auto& modifier : modifiers) {
    if (modifier.method_data.supported_method.empty()) {
      *error_message = "Must specify payment method identifier";
      return false;
    }

    if (modifier.total) {
      if (!ValidateShippingOptionOrPaymentItem(*modifier.total,
                                               error_message)) {
        return false;
      }

      if (modifier.total->amount->value[0] == '-') {
        *error_message = "Total amount value should be non-negative";
        return false;
      }
    }

    if (modifier.additional_display_items.size()) {
      if (!ValidateDisplayItems(modifier.additional_display_items,
                                error_message)) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

bool ValidatePaymentDetails(const PaymentDetails& details,
                            std::string* error_message) {
  if (details.total) {
    if (!ValidateShippingOptionOrPaymentItem(*details.total, error_message)) {
      return false;
    }

    if (details.total->amount->value[0] == '-') {
      *error_message = "Total amount value should be non-negative";
      return false;
    }
  }

  if (details.display_items.size()) {
    if (!ValidateDisplayItems(details.display_items, error_message)) {
      return false;
    }
  }

  if (details.shipping_options.size()) {
    if (!ValidateShippingOptions(details.shipping_options, error_message)) {
      return false;
    }
  }

  if (details.modifiers.size()) {
    if (!ValidatePaymentDetailsModifiers(details.modifiers, error_message)) {
      return false;
    }
  }
  if (!PaymentsValidators::IsValidErrorMsgFormat(details.error, error_message))
    return false;
  return true;
}

}  // namespace payments
