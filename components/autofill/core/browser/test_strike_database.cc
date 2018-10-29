// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

TestStrikeDatabase::TestStrikeDatabase() {}

TestStrikeDatabase::~TestStrikeDatabase() {}

void TestStrikeDatabase::GetStrikes(const std::string key,
                                    const StrikesCallback& outer_callback) {
  outer_callback.Run(GetStrikesForTesting(key));
}

void TestStrikeDatabase::AddStrike(const std::string key,
                                   const StrikesCallback& outer_callback) {
  std::unordered_map<std::string, StrikeData>::iterator it = db_.find(key);
  StrikeData strike_data;
  strike_data.set_last_update_timestamp(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  if (it != db_.end())
    strike_data.set_num_strikes(it->second.num_strikes() + 1);
  else
    strike_data.set_num_strikes(1);
  db_[key] = strike_data;
  outer_callback.Run(strike_data.num_strikes());
}

void TestStrikeDatabase::ClearAllStrikes(
    const ClearStrikesCallback& outer_callback) {
  db_.clear();
  outer_callback.Run(/*success=*/true);
}

void TestStrikeDatabase::ClearAllStrikesForKey(
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
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  db_[key] = strike_data;
}

int TestStrikeDatabase::GetStrikesForTesting(const std::string& key) {
  std::unordered_map<std::string, StrikeData>::iterator it = db_.find(key);
  if (it != db_.end())
    return it->second.num_strikes();
  return 0;
}

}  // namespace autofill
