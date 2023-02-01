// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_PROFILE_UPDATE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_PROFILE_UPDATE_STRIKE_DATABASE_H_

#include <stdint.h>
#include <string>

#include "components/autofill/core/browser/strike_databases/strike_database_base.h"
#include "components/autofill/core/browser/strike_databases/strike_database_integrator_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// Implementation of StrikeDatabaseIntegratorBase for autofill profile updates.
// Records the number of times a user declines updating their Autofill profile
// and stops prompting the user to do so after reaching a strike limit.
class AutofillProfileUpdateStrikeDatabase
    : public StrikeDatabaseIntegratorBase {
 public:
  explicit AutofillProfileUpdateStrikeDatabase(
      StrikeDatabaseBase* strike_database);
  ~AutofillProfileUpdateStrikeDatabase() override;

  absl::optional<size_t> GetMaximumEntries() const override;
  absl::optional<size_t> GetMaximumEntriesAfterCleanup() const override;

  std::string GetProjectPrefix() const override;
  int GetMaxStrikesLimit() const override;
  absl::optional<base::TimeDelta> GetExpiryTimeDelta() const override;
  bool UniqueIdsRequired() const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_PROFILE_UPDATE_STRIKE_DATABASE_H_
