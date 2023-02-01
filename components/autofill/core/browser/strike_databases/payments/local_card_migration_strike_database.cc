// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/local_card_migration_strike_database.h"
#include <limits>

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

const int LocalCardMigrationStrikeDatabase::kStrikesToRemoveWhenLocalCardAdded =
    2;
const int LocalCardMigrationStrikeDatabase::kStrikesToAddWhenBubbleClosed = 3;
const int LocalCardMigrationStrikeDatabase::kStrikesToAddWhenDialogClosed = 6;

LocalCardMigrationStrikeDatabase::LocalCardMigrationStrikeDatabase(
    StrikeDatabase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

LocalCardMigrationStrikeDatabase::~LocalCardMigrationStrikeDatabase() = default;

std::string LocalCardMigrationStrikeDatabase::GetProjectPrefix() const {
  return "LocalCardMigration";
}

int LocalCardMigrationStrikeDatabase::GetMaxStrikesLimit() const {
  return 6;
}

absl::optional<base::TimeDelta>
LocalCardMigrationStrikeDatabase::GetExpiryTimeDelta() const {
  // Ideally, we should be able to annotate cards deselected at migration time
  // as cards the user is not interested in uploading.  Until then, we have been
  // asked to not expire local card migration strikes.
  return absl::nullopt;
}

bool LocalCardMigrationStrikeDatabase::UniqueIdsRequired() const {
  return false;
}

}  // namespace autofill
