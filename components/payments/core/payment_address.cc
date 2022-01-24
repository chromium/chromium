// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_address.h"

#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/browser-payment-api/#paymentaddress-interface
static const char kAddressAddressLine[] = "addressLine";
static const char kAddressCity[] = "city";
static const char kAddressCountry[] = "country";
static const char kAddressDependentLocality[] = "dependentLocality";
static const char kAddressOrganization[] = "organization";
static const char kAddressPhone[] = "phone";
static const char kAddressPostalCode[] = "postalCode";
static const char kAddressRecipient[] = "recipient";
static const char kAddressRegion[] = "region";
static const char kAddressSortingCode[] = "sortingCode";

}  // namespace

base::Value PaymentAddressToValue(const mojom::PaymentAddress& address) {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetStringKey(kAddressCountry, address.country);
  base::Value address_line_list(base::Value::Type::LIST);
  for (const std::string& address_line_string : address.address_line) {
    if (!address_line_string.empty())
      address_line_list.Append(address_line_string);
  }
  result.SetKey(kAddressAddressLine, std::move(address_line_list));
  result.SetStringKey(kAddressRegion, address.region);
  result.SetStringKey(kAddressCity, address.city);
  result.SetStringKey(kAddressDependentLocality, address.dependent_locality);
  result.SetStringKey(kAddressPostalCode, address.postal_code);
  result.SetStringKey(kAddressSortingCode, address.sorting_code);
  result.SetStringKey(kAddressOrganization, address.organization);
  result.SetStringKey(kAddressRecipient, address.recipient);
  result.SetStringKey(kAddressPhone, address.phone);

  return result;
}

}  // namespace payments
