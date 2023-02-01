// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/test_credit_card_save_strike_database.h"

namespace autofill {

TestCreditCardSaveStrikeDatabase::TestCreditCardSaveStrikeDatabase(
    StrikeDatabase* strike_database)
    : CreditCardSaveStrikeDatabase(strike_database) {}

}  // namespace autofill
