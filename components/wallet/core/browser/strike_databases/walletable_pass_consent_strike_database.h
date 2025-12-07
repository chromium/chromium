// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_STRIKE_DATABASES_WALLETABLE_PASS_CONSENT_STRIKE_DATABASE_H_
#define COMPONENTS_WALLET_CORE_BROWSER_STRIKE_DATABASES_WALLETABLE_PASS_CONSENT_STRIKE_DATABASE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "components/strike_database/history_clearable_strike_database.h"

namespace wallet {

// This strike database tracks a global, non-per-site feature and uses a shared
// key.
struct WalletablePassConsentStrikeDatabaseTraits {
  static constexpr std::string_view kName = "WalletablePassConsent";
  static constexpr std::optional<size_t> kMaxStrikeEntities = std::nullopt;
  static constexpr std::optional<size_t> kMaxStrikeEntitiesAfterCleanup =
      std::nullopt;
  static constexpr std::optional<base::TimeDelta> kExpiryTimeDelta =
      std::nullopt;
  static constexpr size_t kMaxStrikeLimit = 2;
  static constexpr bool kUniqueIdRequired = false;
};

class WalletablePassConsentStrikeDatabase
    : public strike_database::HistoryClearableStrikeDatabase<
          WalletablePassConsentStrikeDatabaseTraits> {
 public:
  using strike_database::HistoryClearableStrikeDatabase<
      WalletablePassConsentStrikeDatabaseTraits>::
      HistoryClearableStrikeDatabase;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_STRIKE_DATABASES_WALLETABLE_PASS_CONSENT_STRIKE_DATABASE_H_
