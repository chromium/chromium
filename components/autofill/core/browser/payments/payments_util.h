// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_UTIL_H_

#include <stdint.h>
#include <vector>

#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill {

class PersonalDataManager;

namespace payments {

// Returns the billing customer ID (a.k.a. the customer number) for the Google
// Payments account for this user. Obtains it from the synced data. Returns 0
// if the customer ID was not found.
int64_t GetBillingCustomerId(PersonalDataManager* personal_data_manager);

// Returns if the customer has an existing Google payments account.
bool HasGooglePaymentsAccount(PersonalDataManager* personal_data_manager);

// Checks if |card_number| matches one of the ranges in
// |supported_card_bin_ranges|, inclusive of the start and end boundaries.
// For example, if the range consists of std::pair<34, 36>, then all cards
// with first two digits of 34, 35 and 36 are supported.
bool IsCreditCardNumberSupported(
    const base::string16& card_number,
    const std::vector<std::pair<int, int>>& supported_card_bin_ranges);

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_UTIL_H_
