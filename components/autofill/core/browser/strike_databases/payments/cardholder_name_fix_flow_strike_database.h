// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_CARDHOLDER_NAME_FIX_FLOW_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_CARDHOLDER_NAME_FIX_FLOW_STRIKE_DATABASE_H_

#include <stddef.h>

#include <optional>
#include <string_view>

#include "base/time/time.h"
#include "components/strike_database/simple_strike_database.h"

namespace autofill {

struct CardholderNameFixFlowStrikeDatabaseTraits {
  static constexpr std::string_view kName = "CardholderNameFixFlow";
  static constexpr std::optional<size_t> kMaxStrikeEntities = std::nullopt;
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup =
      std::nullopt;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr std::optional<base::TimeDelta> kExpiryTimeDelta =
      std::nullopt;
  static constexpr bool kUniqueIdRequired = true;
};

class CardholderNameFixFlowStrikeDatabase
    : public strike_database::SimpleStrikeDatabase<
          CardholderNameFixFlowStrikeDatabaseTraits> {
 public:
  using SimpleStrikeDatabase<
      CardholderNameFixFlowStrikeDatabaseTraits>::SimpleStrikeDatabase;

  std::optional<base::TimeDelta> GetRequiredDelaySinceLastStrike()
      const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_CARDHOLDER_NAME_FIX_FLOW_STRIKE_DATABASE_H_
