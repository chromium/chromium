// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/cvc_storage_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

// The maximum number of strikes before we stop offering save CVC dialog.
int kMaximumStrikes = 3;

// The delay required since the last strike before offering another CVC dialog
// attempt.
int kEnforceDelays = 7;

// The number of days until strikes expire for offering save CVC dialog.
constexpr size_t kDaysUntilStrikeExpiry = 183;

CvcStorageStrikeDatabase::CvcStorageStrikeDatabase(
    StrikeDatabase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

std::string CvcStorageStrikeDatabase::GetProjectPrefix() const {
  return "CvcStorage";
}

int CvcStorageStrikeDatabase::GetMaxStrikesLimit() const {
  return kMaximumStrikes;
}

absl::optional<base::TimeDelta> CvcStorageStrikeDatabase::GetExpiryTimeDelta()
    const {
  return base::Days(kDaysUntilStrikeExpiry);
}

bool CvcStorageStrikeDatabase::UniqueIdsRequired() const {
  return true;
}

absl::optional<base::TimeDelta>
CvcStorageStrikeDatabase::GetRequiredDelaySinceLastStrike() const {
  return absl::optional<base::TimeDelta>(base::Days(kEnforceDelays));
}

}  // namespace autofill
