// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_VALUABLES_DATA_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_VALUABLES_DATA_TEST_UTILS_H_

#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"

namespace autofill::test {

// Returns a fully populated loyalty card.
LoyaltyCard CreateLoyaltyCard();
// Returns a fully populated loyalty card different from `CreateLoyaltyCard()`.
LoyaltyCard CreateLoyaltyCard2();

}  // namespace autofill::test

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_VALUABLES_DATA_TEST_UTILS_H_
