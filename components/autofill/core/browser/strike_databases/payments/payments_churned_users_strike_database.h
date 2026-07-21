// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_PAYMENTS_CHURNED_USERS_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_PAYMENTS_CHURNED_USERS_STRIKE_DATABASE_H_

#include <stddef.h>

#include <optional>
#include <string_view>

#include "base/time/time.h"
#include "components/strike_database/simple_strike_database.h"
#include "components/strike_database/strike_database_base.h"

namespace autofill {

// The delay required since the last strike before offering another churned
// user attempt.
inline constexpr int kPaymentsChurnedUsersEnforcedDelayInDays = 7;

struct PaymentsChurnedUsersStrikeDatabaseTraits {
  static constexpr std::string_view kName = "PaymentsChurnedUsers";
  static constexpr std::optional<size_t> kMaxStrikeEntities = std::nullopt;
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup =
      std::nullopt;
  static constexpr size_t kMaxStrikeLimit = 2;
  static constexpr std::optional<base::TimeDelta> kExpiryTimeDelta =
      base::Days(180);
  static constexpr bool kUniqueIdRequired = false;
};

// This is essentially a strike_database::SimpleStrikeDatabase.
class PaymentsChurnedUsersStrikeDatabase
    : public strike_database::SimpleStrikeDatabase<
          PaymentsChurnedUsersStrikeDatabaseTraits> {
 public:
  using SimpleStrikeDatabase<
      PaymentsChurnedUsersStrikeDatabaseTraits>::SimpleStrikeDatabase;

  std::optional<base::TimeDelta> GetRequiredDelaySinceLastStrike()
      const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_PAYMENTS_CHURNED_USERS_STRIKE_DATABASE_H_
