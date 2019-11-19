// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_SAVE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_SAVE_STRIKE_DATABASE_H_

#include "components/autofill/core/browser/payments/credit_card_save_strike_database.h"

namespace autofill {

class TestCreditCardSaveStrikeDatabase : public CreditCardSaveStrikeDatabase {
 public:
  TestCreditCardSaveStrikeDatabase(StrikeDatabase* strike_database);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_CREDIT_CARD_SAVE_STRIKE_DATABASE_H_
