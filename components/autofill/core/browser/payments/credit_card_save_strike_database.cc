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

CreditCardSaveStrikeDatabase::~CreditCardSaveStrikeDatabase() = default;

std::string CreditCardSaveStrikeDatabase::GetProjectPrefix() const {
  return "CreditCardSave";
}

int CreditCardSaveStrikeDatabase::GetMaxStrikesLimit() const {
  return 3;
}

base::Optional<int64_t> CreditCardSaveStrikeDatabase::GetExpiryTimeMicros()
    const {
  // Expiry time is 6 months.
  return static_cast<int64_t>(1000000) * 60 * 60 * 24 * 180;
}

bool CreditCardSaveStrikeDatabase::UniqueIdsRequired() const {
  return true;
}

}  // namespace autofill
