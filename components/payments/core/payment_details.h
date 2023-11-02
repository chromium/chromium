// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_DETAILS_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_DETAILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/payments/core/payment_details_modifier.h"
#include "components/payments/core/payment_item.h"
#include "components/payments/core/payment_shipping_option.h"

// C++ bindings for the PaymentRequest API PaymentDetails. Conforms to the
// following spec:
// https://w3c.github.io/payment-request/#payment-details-dictionaries

namespace payments {

// Details about the requested transaction.
class PaymentDetails {
 public:
  PaymentDetails();
  PaymentDetails(const PaymentDetails& other);
  ~PaymentDetails();

  PaymentDetails& operator=(const PaymentDetails& other);
  bool operator==(const PaymentDetails& other) const;
  bool operator!=(const PaymentDetails& other) const;

  // Populates the properties of this PaymentDetails from |dict|. Returns true
  // if the required values are present. If |requires_total| is true, the total
  // property has to be present.
  bool FromValueDict(const base::Value::Dict& dict, bool requires_total);

  // The unique free-form identifier for this payment request.
  std::string id;

  // The total amount of the payment request.
  std::unique_ptr<PaymentItem> total;

  // Line items for the payment request that the user agent may display. For
  // example, it might include details of products or breakdown of tax and
  // shipping.
  std::vector<PaymentItem> display_items;

  // The different shipping options for the user to choose from. If empty, this
  // indicates that the merchant cannot ship to the current shipping address.
  std::vector<PaymentShippingOption> shipping_options;

  // Modifiers for particular payment method identifiers. For example, it allows
  // adjustment to the total amount based on payment method.
  std::vector<PaymentDetailsModifier> modifiers;

  // If non-empty, this is the error message the user agent should display to
  // the user when the payment request is updated using updateWith.
  std::string error;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_DETAILS_H_
