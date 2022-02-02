// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_method_data.h"

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/payment-method-basic-card/
static const char kMethodDataData[] = "data";
static const char kSupportedMethods[] = "supportedMethods";
static const char kSupportedNetworks[] = "supportedNetworks";

}  // namespace

PaymentMethodData::PaymentMethodData() {}
PaymentMethodData::PaymentMethodData(const PaymentMethodData& other) = default;
PaymentMethodData::~PaymentMethodData() = default;

bool PaymentMethodData::operator==(const PaymentMethodData& other) const {
  return supported_method == other.supported_method && data == other.data &&
         supported_networks == other.supported_networks;
}

bool PaymentMethodData::operator!=(const PaymentMethodData& other) const {
  return !(*this == other);
}

bool PaymentMethodData::FromValue(const base::Value& value) {
  if (!value.is_dict()) {
    return false;
  }

  supported_networks.clear();

  // The value of supportedMethods should be a string.
  const std::string* supported_method_in =
      value.FindStringKey(kSupportedMethods);
  if (!supported_method_in || !base::IsStringASCII(*supported_method_in) ||
      supported_method_in->empty()) {
    return false;
  }
  supported_method = *supported_method_in;

  // Data is optional, but if a dictionary is present, save a stringified
  // version and attempt to parse supportedNetworks.
  const base::Value* data_dict = value.FindDictKey(kMethodDataData);
  if (data_dict) {
    std::string json_data;
    base::JSONWriter::Write(*data_dict, &json_data);
    data = json_data;
    const base::Value* supported_networks_list =
        data_dict->FindListKey(kSupportedNetworks);
    if (supported_networks_list) {
      for (const base::Value& supported_network :
           supported_networks_list->GetListDeprecated()) {
        if (!supported_network.is_string() ||
            !base::IsStringASCII(supported_network.GetString())) {
          return false;
        }
        supported_networks.push_back(supported_network.GetString());
      }
    }
  }
  return true;
}

}  // namespace payments
