// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_database_integrator_test_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

const char kProjectPrefix[] = "StrikeDatabaseIntegratorTest";
const int kMaxStrikesLimit = 6;

StrikeDatabaseIntegratorTestStrikeDatabase::
    StrikeDatabaseIntegratorTestStrikeDatabase(
        StrikeDatabase* strike_database,
        base::Optional<base::TimeDelta> expiry_time_delta)
    : StrikeDatabaseIntegratorTestStrikeDatabase(strike_database) {
  expiry_time_delta_ = expiry_time_delta;
}

StrikeDatabaseIntegratorTestStrikeDatabase::
    StrikeDatabaseIntegratorTestStrikeDatabase(StrikeDatabase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

StrikeDatabaseIntegratorTestStrikeDatabase::
    ~StrikeDatabaseIntegratorTestStrikeDatabase() = default;

std::string StrikeDatabaseIntegratorTestStrikeDatabase::GetProjectPrefix()
    const {
  return kProjectPrefix;
}

int StrikeDatabaseIntegratorTestStrikeDatabase::GetMaxStrikesLimit() const {
  return kMaxStrikesLimit;
}

base::Optional<base::TimeDelta>
StrikeDatabaseIntegratorTestStrikeDatabase::GetExpiryTimeDelta() const {
  return expiry_time_delta_;
}

bool StrikeDatabaseIntegratorTestStrikeDatabase::UniqueIdsRequired() const {
  return unique_ids_required_;
}

void StrikeDatabaseIntegratorTestStrikeDatabase::SetUniqueIdsRequired(
    bool unique_ids_required) {
  unique_ids_required_ = unique_ids_required;
}

base::Optional<size_t>
StrikeDatabaseIntegratorTestStrikeDatabase::GetMaximumEntries() const {
  return maximum_entries_;
}

base::Optional<size_t>
StrikeDatabaseIntegratorTestStrikeDatabase::GetMaximumEntriesAfterCleanup()
    const {
  return maximum_entries_after_cleanup_;
}
}  // namespace autofill
