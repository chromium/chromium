// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_CVC_STORAGE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_CVC_STORAGE_STRIKE_DATABASE_H_

#include <stddef.h>

#include <optional>
#include <string_view>

#include "base/time/time.h"
#include "components/strike_database/simple_strike_database.h"

namespace autofill {

struct CvcStorageStrikeDatabaseTraits {
  static constexpr std::string_view kName = "CvcStorage";
  static constexpr std::optional<size_t> kMaxStrikeEntities;
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(183);
  static constexpr bool kUniqueIdRequired = true;
};

class CvcStorageStrikeDatabase : public strike_database::SimpleStrikeDatabase<
                                     CvcStorageStrikeDatabaseTraits> {
 public:
  using SimpleStrikeDatabase<
      CvcStorageStrikeDatabaseTraits>::SimpleStrikeDatabase;

  std::optional<base::TimeDelta> GetRequiredDelaySinceLastStrike()
      const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_CVC_STORAGE_STRIKE_DATABASE_H_
