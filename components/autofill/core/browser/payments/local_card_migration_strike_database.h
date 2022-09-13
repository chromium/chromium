// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_LOCAL_CARD_MIGRATION_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_LOCAL_CARD_MIGRATION_STRIKE_DATABASE_H_

#include <stdint.h>
#include <string>

#include "components/autofill/core/browser/strike_database.h"
#include "components/autofill/core/browser/strike_database_integrator_base.h"

namespace autofill {

// Implementation of StrikeDatabaseIntegratorBase for local card migrations.
class LocalCardMigrationStrikeDatabase : public StrikeDatabaseIntegratorBase {
 public:
  explicit LocalCardMigrationStrikeDatabase(StrikeDatabase* strike_database);
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

  std::string GetProjectPrefix() const override;
  int GetMaxStrikesLimit() const override;
  absl::optional<base::TimeDelta> GetExpiryTimeDelta() const override;
  bool UniqueIdsRequired() const override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_LOCAL_CARD_MIGRATION_STRIKE_DATABASE_H_
