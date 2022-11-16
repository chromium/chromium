// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_update_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

// Limit the number of profiles for which an update os blocked.
constexpr size_t kMaxStrikeEntities = 100;

// Once the limit of profiles is reached, delete 30 to create a bit of headroom.
constexpr size_t kMaxStrikeEntitiesAfterCleanup = 70;

// The number of days it takes for strikes to expire.
constexpr int kNumberOfDaysToExpire = 180;

// The strike limit for suppressing update prompts.
constexpr int kStrikeLimit = 3;

AutofillProfileUpdateStrikeDatabase::AutofillProfileUpdateStrikeDatabase(
    StrikeDatabaseBase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

AutofillProfileUpdateStrikeDatabase::~AutofillProfileUpdateStrikeDatabase() =
    default;

absl::optional<size_t> AutofillProfileUpdateStrikeDatabase::GetMaximumEntries()
    const {
  return absl::make_optional(kMaxStrikeEntities);
}

absl::optional<size_t>
AutofillProfileUpdateStrikeDatabase::GetMaximumEntriesAfterCleanup() const {
  return absl::make_optional(kMaxStrikeEntitiesAfterCleanup);
}

std::string AutofillProfileUpdateStrikeDatabase::GetProjectPrefix() const {
  return "AutofillProfileUpdate";
}

int AutofillProfileUpdateStrikeDatabase::GetMaxStrikesLimit() const {
  return kStrikeLimit;
}

absl::optional<base::TimeDelta>
AutofillProfileUpdateStrikeDatabase::GetExpiryTimeDelta() const {
  return base::Days(kNumberOfDaysToExpire);
}

bool AutofillProfileUpdateStrikeDatabase::UniqueIdsRequired() const {
  return true;
}

}  // namespace autofill
