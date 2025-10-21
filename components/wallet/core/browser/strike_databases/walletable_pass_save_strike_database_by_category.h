// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_STRIKE_DATABASES_WALLETABLE_PASS_SAVE_STRIKE_DATABASE_BY_CATEGORY_H_
#define COMPONENTS_WALLET_CORE_BROWSER_STRIKE_DATABASES_WALLETABLE_PASS_SAVE_STRIKE_DATABASE_BY_CATEGORY_H_

#include <string>

#include "base/time/time.h"
#include "components/strike_database/simple_strike_database.h"

namespace wallet {
struct WalletablePassSaveStrikeDatabaseByCategoryTraits {
  static constexpr std::string_view kName = "WalletablePassSave";
  static constexpr size_t kMaxStrikeEntities = 100;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 70;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(180);
  static constexpr bool kUniqueIdRequired = true;
};

using WalletablePassSaveStrikeDatabaseByCategory =
    strike_database::SimpleStrikeDatabase<
        WalletablePassSaveStrikeDatabaseByCategoryTraits>;

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_STRIKE_DATABASES_WALLETABLE_PASS_SAVE_STRIKE_DATABASE_BY_CATEGORY_H_
