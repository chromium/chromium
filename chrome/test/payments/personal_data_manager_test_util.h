// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PAYMENTS_PERSONAL_DATA_MANAGER_TEST_UTIL_H_
#define CHROME_TEST_PAYMENTS_PERSONAL_DATA_MANAGER_TEST_UTIL_H_

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace content {
class BrowserContext;
}  // namespace content

namespace payments {
namespace test {

// Adds |autofill_profile| to the personal data manager and blocks while the
// data is being saved.
void AddAutofillProfile(content::BrowserContext* browser_context,
                        const autofill::AutofillProfile& autofill_profile);

// Adds |card| to the personal data manager and blocks while the data is being
// saved.
void AddCreditCard(content::BrowserContext* browser_context,
                   const autofill::CreditCard& card);

}  // namespace test
}  // namespace payments

#endif  // CHROME_TEST_PAYMENTS_PERSONAL_DATA_MANAGER_TEST_UTIL_H_
