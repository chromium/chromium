// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_STRIKE_DATABASE_INTEGRATOR_TEST_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_STRIKE_DATABASE_INTEGRATOR_TEST_STRIKE_DATABASE_H_

#include <stdint.h>
#include <string>

#include "base/time/time.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database_integrator_base.h"

namespace autofill {

// Mock per-project implementation of StrikeDatabase to test the functions in
// StrikeDatabaseIntegrator.
class StrikeDatabaseIntegratorTestStrikeDatabase
    : public StrikeDatabaseIntegratorBase {
 public:
  StrikeDatabaseIntegratorTestStrikeDatabase(
      StrikeDatabase* strike_database,
      std::optional<base::TimeDelta> expiry_time_delta);
  explicit StrikeDatabaseIntegratorTestStrikeDatabase(
      StrikeDatabase* strike_database);
  // This constructor initializes the TestStrikeDatabase with a non-default
  // project prefix.
  StrikeDatabaseIntegratorTestStrikeDatabase(
      StrikeDatabase* strike_database,
      std::optional<base::TimeDelta> expiry_time_delta,
      std::string& project_prefix);
  ~StrikeDatabaseIntegratorTestStrikeDatabase() override;

  std::optional<size_t> GetMaximumEntries() const override;
  std::optional<size_t> GetMaximumEntriesAfterCleanup() const override;

  std::string GetProjectPrefix() const override;
  int GetMaxStrikesLimit() const override;
  std::optional<base::TimeDelta> GetExpiryTimeDelta() const override;
  bool UniqueIdsRequired() const override;
  std::optional<base::TimeDelta> GetRequiredDelaySinceLastStrike()
      const override;

  void SetUniqueIdsRequired(bool unique_ids_required);
  void SetRequiredDelaySinceLastStrike(
      base::TimeDelta required_delay_since_last_strike);

 private:
  bool unique_ids_required_ = false;
  std::optional<base::TimeDelta> expiry_time_delta_ = base::Days(365);

  std::optional<size_t> maximum_entries_ = 10;
  std::optional<size_t> maximum_entries_after_cleanup_ = 5;
  std::string project_prefix_ = "StrikeDatabaseIntegratorTest";
  std::optional<base::TimeDelta> required_delay_since_last_strike_ =
      std::nullopt;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_STRIKE_DATABASE_INTEGRATOR_TEST_STRIKE_DATABASE_H_
