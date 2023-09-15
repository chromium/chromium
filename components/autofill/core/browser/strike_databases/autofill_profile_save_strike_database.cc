// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/autofill_profile_save_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

void AutofillProfileSaveStrikeDatabase::ClearStrikesByOrigin(
    const std::set<std::string>& hosts_to_delete) {
  ClearStrikesByOriginAndTimeInternal(hosts_to_delete, base::Time::Min(),
                                      base::Time::Max());
}

void AutofillProfileSaveStrikeDatabase::ClearStrikesByOriginAndTimeInternal(
    const std::set<std::string>& hosts_to_delete,
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

  for (auto const& [key, strike_data] : GetStrikeCache()) {
    std::string strike_id = GetIdFromKey(key);
    if (strike_id.empty()) {
      continue;
    }

    base::Time last_update = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(strike_data.last_update_timestamp()));

    // Check if the time stamp of the record is within deletion range and if the
    // domain is deleted.
    if (last_update >= delete_begin && last_update <= delete_end &&
        hosts_to_delete.count(strike_id) != 0) {
      keys_to_delete.push_back(key);
    }
  }

  ClearStrikesForKeys(keys_to_delete);
}

}  // namespace autofill
