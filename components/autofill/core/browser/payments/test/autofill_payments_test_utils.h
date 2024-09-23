// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_AUTOFILL_PAYMENTS_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_AUTOFILL_PAYMENTS_TEST_UTILS_H_

#include <vector>

namespace autofill {

class AutofillProfile;

namespace payments {

// Provides a set of test profiles for use in Autofill Payments tests.
std::vector<AutofillProfile> BuildTestProfiles();

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_AUTOFILL_PAYMENTS_TEST_UTILS_H_
