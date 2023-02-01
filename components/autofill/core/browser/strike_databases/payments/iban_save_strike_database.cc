// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

IBANSaveStrikeDatabase::IBANSaveStrikeDatabase(StrikeDatabase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

std::string IBANSaveStrikeDatabase::GetProjectPrefix() const {
  return "IBANSave";
}

int IBANSaveStrikeDatabase::GetMaxStrikesLimit() const {
  return 3;
}

absl::optional<base::TimeDelta> IBANSaveStrikeDatabase::GetExpiryTimeDelta()
    const {
  // Expiry time is 6 months.
  return base::Days(183);
}

bool IBANSaveStrikeDatabase::UniqueIdsRequired() const {
  return true;
}

}  // namespace autofill
