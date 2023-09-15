// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_PROFILE_MIGRATION_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_PROFILE_MIGRATION_STRIKE_DATABASE_H_

#include "components/autofill/core/browser/strike_databases/simple_autofill_strike_database.h"

namespace autofill {

struct AutofillProfileMigrationStrikeDatabaseTraits {
  static constexpr std::string_view kName = "AutofillProfileMigration";
  static constexpr size_t kMaxStrikeEntities = 100;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 70;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(180);
  static constexpr bool kUniqueIdRequired = true;
};

// Records the number of times a user declines migrating their
// `kLocalOrSyncable` profile to `kAccount` profile and stops prompting the user
// to do so after reaching a strike limit.
using AutofillProfileMigrationStrikeDatabase =
    SimpleAutofillStrikeDatabase<AutofillProfileMigrationStrikeDatabaseTraits>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_AUTOFILL_PROFILE_MIGRATION_STRIKE_DATABASE_H_
