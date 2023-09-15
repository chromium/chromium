// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_LOCAL_CARD_MIGRATION_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_LOCAL_CARD_MIGRATION_STRIKE_DATABASE_H_

#include "components/autofill/core/browser/strike_databases/simple_autofill_strike_database.h"

namespace autofill {

struct LocalCardMigrationStrikeDatabaseTraits {
  static constexpr std::string_view kName = "LocalCardMigration";
  static constexpr absl::optional<size_t> kMaxStrikeEntities = absl::nullopt;
  static constexpr absl::optional<size_t> kMaxStrikeEntitiesAfterCleanup =
      absl::nullopt;
  static constexpr size_t kMaxStrikeLimit = 6;
  static constexpr absl::optional<base::TimeDelta> kExpiryTimeDelta =
      absl::nullopt;
  static constexpr bool kUniqueIdRequired = false;
};

class LocalCardMigrationStrikeDatabase
    : public SimpleAutofillStrikeDatabase<
          LocalCardMigrationStrikeDatabaseTraits> {
 public:
  using SimpleAutofillStrikeDatabase<
      LocalCardMigrationStrikeDatabaseTraits>::SimpleAutofillStrikeDatabase;

  static constexpr int kStrikesToRemoveWhenLocalCardAdded = 2;
  static constexpr int kStrikesToAddWhenBubbleClosed = 3;
  static constexpr int kStrikesToAddWhenDialogClosed = 6;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_STRIKE_DATABASES_PAYMENTS_LOCAL_CARD_MIGRATION_STRIKE_DATABASE_H_
