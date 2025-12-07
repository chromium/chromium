// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_STRIKE_DATABASES_WALLETABLE_PASS_SAVE_STRIKE_DATABASE_BY_HOST_H_
#define COMPONENTS_WALLET_CORE_BROWSER_STRIKE_DATABASES_WALLETABLE_PASS_SAVE_STRIKE_DATABASE_BY_HOST_H_

#include <string>
#include <string_view>

#include "base/time/time.h"
#include "components/strike_database/history_clearable_strike_database.h"

namespace wallet {
struct WalletablePassSaveStrikeDatabaseByHostTraits {
  static constexpr std::string_view kName = "WalletablePassSaveByHost";
  static constexpr size_t kMaxStrikeEntities = 200;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 150;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(180);
  static constexpr bool kUniqueIdRequired = true;
};

class WalletablePassSaveStrikeDatabaseByHost
    : public strike_database::HistoryClearableStrikeDatabase<
          WalletablePassSaveStrikeDatabaseByHostTraits> {
 public:
  using strike_database::HistoryClearableStrikeDatabase<
      WalletablePassSaveStrikeDatabaseByHostTraits>::
      HistoryClearableStrikeDatabase;

  // Returns an id for use in the strike database.
  static std::string GetId(std::string_view category, std::string_view host);
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_STRIKE_DATABASES_WALLETABLE_PASS_SAVE_STRIKE_DATABASE_BY_HOST_H_
