// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASE_INTEGRATOR_TEST_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASE_INTEGRATOR_TEST_STRIKE_DATABASE_H_

#include <stdint.h>
#include <string>

#include "components/autofill/core/browser/strike_database.h"
#include "components/autofill/core/browser/strike_database_integrator_base.h"

namespace autofill {

// Mock per-project implementation of StrikeDatabase to test the functions in
// StrikeDatabaseIntegrator.
class StrikeDatabaseIntegratorTestStrikeDatabase
    : public StrikeDatabaseIntegratorBase {
 public:
  StrikeDatabaseIntegratorTestStrikeDatabase(
      StrikeDatabase* strike_database,
      base::Optional<base::TimeDelta> expiry_time_delta);
  explicit StrikeDatabaseIntegratorTestStrikeDatabase(
      StrikeDatabase* strike_database);
  ~StrikeDatabaseIntegratorTestStrikeDatabase() override;

  base::Optional<size_t> GetMaximumEntries() const override;
  base::Optional<size_t> GetMaximumEntriesAfterCleanup() const override;

  std::string GetProjectPrefix() const override;
  int GetMaxStrikesLimit() const override;
  base::Optional<base::TimeDelta> GetExpiryTimeDelta() const override;
  bool UniqueIdsRequired() const override;

  void SetUniqueIdsRequired(bool unique_ids_required);

 private:
  bool unique_ids_required_ = false;
  base::Optional<base::TimeDelta> expiry_time_delta_ =
      base::TimeDelta::FromDays(365);

  base::Optional<size_t> maximum_entries_ = 10;
  base::Optional<size_t> maximum_entries_after_cleanup_ = 5;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASE_INTEGRATOR_TEST_STRIKE_DATABASE_H_
