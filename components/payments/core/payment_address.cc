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

std::unique_ptr<base::DictionaryValue> PaymentAddressToDictionaryValue(
    const mojom::PaymentAddress& address) {
  auto result = std::make_unique<base::DictionaryValue>();
  result->SetString(kAddressCountry, address.country);
  base::ListValue address_line_list;
  for (const std::string& address_line_string : address.address_line) {
    if (!address_line_string.empty())
      address_line_list.AppendString(address_line_string);
  }
  result->SetKey(kAddressAddressLine, std::move(address_line_list));
  result->SetString(kAddressRegion, address.region);
  result->SetString(kAddressCity, address.city);
  result->SetString(kAddressDependentLocality, address.dependent_locality);
  result->SetString(kAddressPostalCode, address.postal_code);
  result->SetString(kAddressSortingCode, address.sorting_code);
  result->SetString(kAddressOrganization, address.organization);
  result->SetString(kAddressRecipient, address.recipient);
  result->SetString(kAddressPhone, address.phone);

  return result;
}

}  // namespace payments
