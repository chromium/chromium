// Copyright 2018 The Chromium Authors
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

absl::optional<base::TimeDelta>
CreditCardSaveStrikeDatabase::GetExpiryTimeDelta() const {
  // Expiry time is 6 months.
  return base::Days(183);
}

bool CreditCardSaveStrikeDatabase::UniqueIdsRequired() const {
  return true;
}

}  // namespace autofill
