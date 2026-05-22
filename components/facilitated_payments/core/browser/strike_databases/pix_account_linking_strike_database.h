// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_STRIKE_DATABASES_PIX_ACCOUNT_LINKING_STRIKE_DATABASE_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_STRIKE_DATABASES_PIX_ACCOUNT_LINKING_STRIKE_DATABASE_H_

#include <cstddef>
#include <optional>
#include <string_view>

#include "base/time/time.h"
#include "components/strike_database/simple_strike_database.h"

namespace payments::facilitated {

// Max 3 strikes, no expiry, 7 days delay between strikes.
struct PixAccountLinkingStrikeDatabaseTraits {
  static constexpr std::string_view kName = "PixAccountLinking";
  // No limit on the number of unique websites (origins) that can have strikes.
  static constexpr std::optional<size_t> kMaxStrikeEntities = std::nullopt;
  // No limit on the number of websites to keep after a cleanup operation.
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup =
      std::nullopt;
  static constexpr size_t kMaxStrikeLimit = 3;
  // Strikes never expire. This ensures that once a user has reached the
  // maximum 3 number of strikes, they are permanently blocked from seeing
  // the prompt.
  static constexpr std::optional<base::TimeDelta> kExpiryTimeDelta =
      std::nullopt;
  static constexpr bool kUniqueIdRequired = false;
};

class PixAccountLinkingStrikeDatabase
    : public strike_database::SimpleStrikeDatabase<
          PixAccountLinkingStrikeDatabaseTraits> {
 public:
  static constexpr base::TimeDelta kDelaySinceLastStrike = base::Days(7);

  using SimpleStrikeDatabase::SimpleStrikeDatabase;

  // Override to enforce 1 week pause after each strike.
  std::optional<base::TimeDelta> GetRequiredDelaySinceLastStrike()
      const override;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_STRIKE_DATABASES_PIX_ACCOUNT_LINKING_STRIKE_DATABASE_H_
