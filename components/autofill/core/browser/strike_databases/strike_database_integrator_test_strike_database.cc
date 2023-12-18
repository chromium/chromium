// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/strike_database_integrator_test_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

const int kMaxStrikesLimit = 6;

StrikeDatabaseIntegratorTestStrikeDatabase::
    StrikeDatabaseIntegratorTestStrikeDatabase(
        StrikeDatabase* strike_database,
        std::optional<base::TimeDelta> expiry_time_delta)
    : StrikeDatabaseIntegratorTestStrikeDatabase(strike_database) {
  expiry_time_delta_ = expiry_time_delta;
}

StrikeDatabaseIntegratorTestStrikeDatabase::
    StrikeDatabaseIntegratorTestStrikeDatabase(StrikeDatabase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

StrikeDatabaseIntegratorTestStrikeDatabase::
    StrikeDatabaseIntegratorTestStrikeDatabase(
        StrikeDatabase* strike_database,
        std::optional<base::TimeDelta> expiry_time_delta,
        std::string& project_prefix)
    : StrikeDatabaseIntegratorTestStrikeDatabase(strike_database,
                                                 expiry_time_delta) {
  project_prefix_ = project_prefix;
}

StrikeDatabaseIntegratorTestStrikeDatabase::
    ~StrikeDatabaseIntegratorTestStrikeDatabase() = default;

std::string StrikeDatabaseIntegratorTestStrikeDatabase::GetProjectPrefix()
    const {
  return project_prefix_;
}

int StrikeDatabaseIntegratorTestStrikeDatabase::GetMaxStrikesLimit() const {
  return kMaxStrikesLimit;
}

std::optional<base::TimeDelta>
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

void StrikeDatabaseIntegratorTestStrikeDatabase::
    SetRequiredDelaySinceLastStrike(
        base::TimeDelta required_delay_since_last_strike) {
  required_delay_since_last_strike_ = required_delay_since_last_strike;
}

std::optional<size_t>
StrikeDatabaseIntegratorTestStrikeDatabase::GetMaximumEntries() const {
  return maximum_entries_;
}

std::optional<size_t>
StrikeDatabaseIntegratorTestStrikeDatabase::GetMaximumEntriesAfterCleanup()
    const {
  return maximum_entries_after_cleanup_;
}

std::optional<base::TimeDelta>
StrikeDatabaseIntegratorTestStrikeDatabase::GetRequiredDelaySinceLastStrike()
    const {
  return required_delay_since_last_strike_;
}

}  // namespace autofill
