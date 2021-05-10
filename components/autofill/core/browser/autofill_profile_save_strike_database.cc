// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_save_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "url/gurl.h"

namespace autofill {

// Limit the number of domains for which the import of new profiles is disabled.
constexpr size_t kMaxStrikeEntities = 200;

// Once the limit of domains is reached, delete 50 to create a bit of headroom.
constexpr size_t kMaxStrikeEntitiesAfterCleanup = 150;

AutofillProfileSaveStrikeDatabase::AutofillProfileSaveStrikeDatabase(
    StrikeDatabase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

AutofillProfileSaveStrikeDatabase::~AutofillProfileSaveStrikeDatabase() =
    default;

base::Optional<size_t> AutofillProfileSaveStrikeDatabase::GetMaximumEntries()
    const {
  return base::make_optional(kMaxStrikeEntities);
}

base::Optional<size_t>
AutofillProfileSaveStrikeDatabase::GetMaximumEntriesAfterCleanup() const {
  return base::make_optional(kMaxStrikeEntitiesAfterCleanup);
}

std::string AutofillProfileSaveStrikeDatabase::GetProjectPrefix() const {
  return "AutofillProfileSave";
}

int AutofillProfileSaveStrikeDatabase::GetMaxStrikesLimit() const {
  return 3;
}

base::Optional<base::TimeDelta>
AutofillProfileSaveStrikeDatabase::GetExpiryTimeDelta() const {
  // Expiry time is 6 months.
  return base::TimeDelta::FromDays(183);
}

bool AutofillProfileSaveStrikeDatabase::UniqueIdsRequired() const {
  return true;
}

void AutofillProfileSaveStrikeDatabase::RemoveStrikesByOriginAndTimeInternal(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  if (delete_begin.is_null()) {
    delete_begin = base::Time::Min();
  }

  if (delete_end.is_null()) {
    delete_end = base::Time::Max();
  }

  std::vector<std::string> keys_to_delete;
  keys_to_delete.reserve(GetStrikeCache().size());

  for (auto const& entry : GetStrikeCache()) {
    std::string strike_id = GetIdFromKey(entry.first);
    if (strike_id.empty()) {
      continue;
    }

    GURL host = GURL(strike_id);

    // If the id cannot be converted to a valid host delete it anyway.
    if (!host.is_valid()) {
      keys_to_delete.push_back(entry.first);
      continue;
    }

    base::Time last_update = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(
            entry.second.last_update_timestamp()));

    // Check if the time stamp of the record is within deletion range and if
    // either there is no origin filter, or if the filter returns true for the
    // host.
    if (last_update >= delete_begin && last_update <= delete_end &&
        (origin_filter.is_null() || origin_filter.Run(host))) {
      keys_to_delete.push_back(entry.first);
    }
  }

  ClearStrikesForKeys(keys_to_delete);
}

}  // namespace autofill
