// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CUSTOMER_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CUSTOMER_DATA_H_

#include <string>

namespace autofill {

// Represents the Google Payments customer data.
struct PaymentsCustomerData {
  explicit PaymentsCustomerData(const std::string& customer_id)
      : customer_id(customer_id) {}

  bool operator==(const PaymentsCustomerData&) const = default;

  // The identifier by which a Google Payments account is identified.
  std::string customer_id;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CUSTOMER_DATA_H_
