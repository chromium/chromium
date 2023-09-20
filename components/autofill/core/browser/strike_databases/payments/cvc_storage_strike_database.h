// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_CVC_STORAGE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_CVC_STORAGE_STRIKE_DATABASE_H_

#include "components/autofill/core/browser/strike_databases/simple_autofill_strike_database.h"

namespace autofill {

struct CvcStorageStrikeDatabaseTraits {
  static constexpr std::string_view kName = "CvcStorage";
  static constexpr absl::optional<size_t> kMaxStrikeEntities = absl::nullopt;
  static constexpr absl::optional<size_t> kMaxStrikeEntitiesAfterCleanup =
      absl::nullopt;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(183);
  static constexpr bool kUniqueIdRequired = true;
};

class CvcStorageStrikeDatabase
    : public SimpleAutofillStrikeDatabase<CvcStorageStrikeDatabaseTraits> {
 public:
  using SimpleAutofillStrikeDatabase<
      CvcStorageStrikeDatabaseTraits>::SimpleAutofillStrikeDatabase;

  absl::optional<base::TimeDelta> GetRequiredDelaySinceLastStrike()
      const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_CVC_STORAGE_STRIKE_DATABASE_H_
