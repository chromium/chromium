// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

TestStrikeDatabase::TestStrikeDatabase() = default;

TestStrikeDatabase::~TestStrikeDatabase() = default;

void TestStrikeDatabase::GetProtoStrikes(
    const std::string& key,
    const StrikesCallback& outer_callback) {
  outer_callback.Run(GetStrikesForTesting(key));
}

void TestStrikeDatabase::ClearAllProtoStrikes(
    const ClearStrikesCallback& outer_callback) {
  db_.clear();
  outer_callback.Run(/*success=*/true);
}

void TestStrikeDatabase::ClearAllProtoStrikesForKey(
    const std::string& key,
    const ClearStrikesCallback& outer_callback) {
  db_.erase(key);
  outer_callback.Run(/*success=*/true);
}

void TestStrikeDatabase::AddEntryWithNumStrikes(const std::string& key,
                                                int num_strikes) {
  StrikeData strike_data;
  strike_data.set_num_strikes(num_strikes);
  strike_data.set_last_update_timestamp(
      AutofillClock::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  db_[key] = strike_data;
}

int TestStrikeDatabase::GetStrikesForTesting(const std::string& key) {
  std::unordered_map<std::string, StrikeData>::iterator it = db_.find(key);
  if (it != db_.end()) {
    return it->second.num_strikes();
  }
  return 0;
}

}  // namespace autofill
