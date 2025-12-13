// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_STRIKE_DATABASES_PAYMENT_LINK_SUGGESTION_STRIKE_DATABASE_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_STRIKE_DATABASES_PAYMENT_LINK_SUGGESTION_STRIKE_DATABASE_H_

#include <cstddef>
#include <optional>
#include <string_view>

#include "base/time/time.h"
#include "components/strike_database/simple_strike_database.h"
#include "components/strike_database/strike_database.h"

namespace payments::facilitated {

// Max 5 strikes and one strike expires every 60 days.
struct PaymentLinkSuggestionStrikeDatabaseTraits {
  static constexpr std::string_view kName = "PaymentLinkSuggestion";
  static constexpr std::optional<size_t> kMaxStrikeEntities = std::nullopt;
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup =
      std::nullopt;
  static constexpr size_t kMaxStrikeLimit = 5;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(60);
  static constexpr bool kUniqueIdRequired = false;
};

// Strike database for payment link suggestions.
using PaymentLinkSuggestionStrikeDatabase =
    strike_database::SimpleStrikeDatabase<
        PaymentLinkSuggestionStrikeDatabaseTraits>;

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_STRIKE_DATABASES_PAYMENT_LINK_SUGGESTION_STRIKE_DATABASE_H_
