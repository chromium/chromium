// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_METHOD_DATA_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_METHOD_DATA_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace base {
class Value;
}

namespace payments {

// A set of supported payment methods and any associated payment method specific
// data for those methods.
class PaymentMethodData {
 public:
  PaymentMethodData();
  PaymentMethodData(const PaymentMethodData& other);
  ~PaymentMethodData();

  bool operator==(const PaymentMethodData& other) const;
  bool operator!=(const PaymentMethodData& other) const;

  // Populates the properties of this PaymentMethodData from |dict|. Returns
  // true if the required values are present.
  bool FromValueDict(const base::Value::Dict& dict);

  // Payment method identifier for payment method that the merchant web site
  // accepts.
  std::string supported_method;

  // A JSON-serialized object that provides optional information that might be
  // needed by the supported payment methods.
  std::string data;

  // When the methods include "basic-card", a list of networks and types that
  // are supported.
  std::vector<std::string> supported_networks;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_METHOD_DATA_H_
