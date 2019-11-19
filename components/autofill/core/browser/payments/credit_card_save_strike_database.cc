// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_save_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

CreditCardSaveStrikeDatabase::CreditCardSaveStrikeDatabase(
    StrikeDatabase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

CreditCardSaveStrikeDatabase::~CreditCardSaveStrikeDatabase() {}

std::string CreditCardSaveStrikeDatabase::GetProjectPrefix() {
  return "CreditCardSave";
}

int CreditCardSaveStrikeDatabase::GetMaxStrikesLimit() {
  return 3;
}

long long CreditCardSaveStrikeDatabase::GetExpiryTimeMicros() {
  // Expiry time is 6 months.
  return (long long)1000000 * 60 * 60 * 24 * 180;
}

bool CreditCardSaveStrikeDatabase::UniqueIdsRequired() {
  return true;
}

}  // namespace autofill
