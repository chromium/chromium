// Copyright 2017 The Chromium Authors
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

base::Value::Dict PaymentAddressToValueDict(
    const mojom::PaymentAddress& address) {
  base::Value::Dict result;
  result.Set(kAddressCountry, address.country);
  base::Value::List address_line_list;
  for (const std::string& address_line_string : address.address_line) {
    if (!address_line_string.empty())
      address_line_list.Append(address_line_string);
  }
  result.Set(kAddressAddressLine, std::move(address_line_list));
  result.Set(kAddressRegion, address.region);
  result.Set(kAddressCity, address.city);
  result.Set(kAddressDependentLocality, address.dependent_locality);
  result.Set(kAddressPostalCode, address.postal_code);
  result.Set(kAddressSortingCode, address.sorting_code);
  result.Set(kAddressOrganization, address.organization);
  result.Set(kAddressRecipient, address.recipient);
  result.Set(kAddressPhone, address.phone);

  return result;
}

}  // namespace payments
