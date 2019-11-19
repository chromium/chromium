// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_LOCAL_CARD_MIGRATION_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_LOCAL_CARD_MIGRATION_STRIKE_DATABASE_H_

#include <string>

#include "components/autofill/core/browser/payments/strike_database.h"
#include "components/autofill/core/browser/payments/strike_database_integrator_base.h"

namespace autofill {

// Implementation of StrikeDatabaseIntegratorBase for local card migrations.
class LocalCardMigrationStrikeDatabase : public StrikeDatabaseIntegratorBase {
 public:
  LocalCardMigrationStrikeDatabase(StrikeDatabase* strike_database);
  ~LocalCardMigrationStrikeDatabase() override;

  // Strikes to remove when user adds new local card.
  static const int kStrikesToRemoveWhenLocalCardAdded;
  // Strikes to add when  user closes LocalCardMigrationBubble.
  static const int kStrikesToAddWhenBubbleClosed;
  // Strikes to add when user closes LocalCardMigrationDialog.
  static const int kStrikesToAddWhenDialogClosed;
  // Number of strikes to add when user de-selected some local cards during
  // migration.
  static const int kStrikesToAddWhenCardsDeselectedAtMigration;

  std::string GetProjectPrefix() override;
  int GetMaxStrikesLimit() override;
  long long GetExpiryTimeMicros() override;
  bool UniqueIdsRequired() override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_LOCAL_CARD_MIGRATION_STRIKE_DATABASE_H_
