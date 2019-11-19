// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/local_card_migration_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

const int LocalCardMigrationStrikeDatabase::kStrikesToRemoveWhenLocalCardAdded =
    2;
const int LocalCardMigrationStrikeDatabase::kStrikesToAddWhenBubbleClosed = 2;
const int LocalCardMigrationStrikeDatabase::kStrikesToAddWhenDialogClosed = 3;
const int LocalCardMigrationStrikeDatabase::
    kStrikesToAddWhenCardsDeselectedAtMigration = 3;

LocalCardMigrationStrikeDatabase::LocalCardMigrationStrikeDatabase(
    StrikeDatabase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

LocalCardMigrationStrikeDatabase::~LocalCardMigrationStrikeDatabase() {}

std::string LocalCardMigrationStrikeDatabase::GetProjectPrefix() {
  return "LocalCardMigration";
}

int LocalCardMigrationStrikeDatabase::GetMaxStrikesLimit() {
  return 6;
}

long long LocalCardMigrationStrikeDatabase::GetExpiryTimeMicros() {
  // Expiry time is 1 year.
  return (long long)1000000 * 60 * 60 * 24 * 365;
}

bool LocalCardMigrationStrikeDatabase::UniqueIdsRequired() {
  return false;
}

}  // namespace autofill
