// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_migration_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

// Limit the number of profiles for which a migration is blocked.
constexpr size_t kMaxStrikeEntities = 100;

// Once the limit of blocked profiles is reached, delete 30 least recently
// blocked profiles to create a bit of headroom.
constexpr size_t kMaxStrikeEntitiesAfterCleanup = 70;

// The number of days it takes for strikes to expire.
constexpr int kNumberOfDaysToExpire = 180;

// The strike limit for suppressing migration prompts.
constexpr int kStrikeLimit = 3;

AutofillProfileMigrationStrikeDatabase::AutofillProfileMigrationStrikeDatabase(
    StrikeDatabaseBase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

AutofillProfileMigrationStrikeDatabase::
    ~AutofillProfileMigrationStrikeDatabase() = default;

absl::optional<size_t>
AutofillProfileMigrationStrikeDatabase::GetMaximumEntries() const {
  return kMaxStrikeEntities;
}

absl::optional<size_t>
AutofillProfileMigrationStrikeDatabase::GetMaximumEntriesAfterCleanup() const {
  return kMaxStrikeEntitiesAfterCleanup;
}

std::string AutofillProfileMigrationStrikeDatabase::GetProjectPrefix() const {
  return "AutofillProfileMigration";
}

int AutofillProfileMigrationStrikeDatabase::GetMaxStrikesLimit() const {
  return kStrikeLimit;
}

absl::optional<base::TimeDelta>
AutofillProfileMigrationStrikeDatabase::GetExpiryTimeDelta() const {
  return base::Days(kNumberOfDaysToExpire);
}

bool AutofillProfileMigrationStrikeDatabase::UniqueIdsRequired() const {
  return true;
}

}  // namespace autofill
