// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PASSES_PASSES_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PASSES_PASSES_TABLE_H_

#include <optional>
#include <string_view>
#include <vector>

#include "components/autofill/core/browser/data_model/passes/loyalty_card.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace autofill {

// This class manages non-payments data types coming from Google Wallet within
// the SQLite database passed in the constructor. It expects the following
// schema:
//
// -----------------------------------------------------------------------------
// loyalty_cards
//
//   guid                         Uniquely identifies the loyalty card instance
//                                (primary key).
//   merchant_name                The name of the loyalty card provider e.g.
//                                "Deutsche Bahn".
//   program_name                 The name of the program from the loyalty card
//                                provider e.g. "BahnBonus".
//   program_logo                 The url of the logo icon for the card.
//   unmasked_loyalty_card_suffix A string representation of the unmasked
//                                loyalty card number suffix.
class PassesTable : public WebDatabaseTable {
 public:
  PassesTable();

  PassesTable(const PassesTable&) = delete;
  PassesTable& operator=(const PassesTable&) = delete;

  ~PassesTable() override;

  // Retrieves the PassesTable* owned by `db`.
  static PassesTable* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Returns all loyalty cards stored in the database.
  std::vector<LoyaltyCard> GetLoyaltyCards() const;

  // Inserts a new or updates an existing loyalty card into the database using
  // the loyalty card id as a primary key. Returns `true` if the database
  // operation succeeded. Returns `false` if the loyalty card's program logo URL
  // is invalid or the database operation fails.
  bool AddOrUpdateLoyaltyCard(const LoyaltyCard& loyalty_card) const;

  // Attempts to retrieve a loyalty card from the database using the
  // `loyalty_card_id` as a unique identifier. Returns `std::nullopt` if there's
  // no loyalty card with `loyalty_card_id` in the database.
  std::optional<LoyaltyCard> GetLoyaltyCardById(
      std::string_view loyalty_card_id) const;

  // Removes the loyalty card from the database using `loyalty_card_id` as a
  // unique identifier. Returns `true` if the operation succeeded.
  bool RemoveLoyaltyCard(std::string_view loyalty_card_id);

  // Removes all loyalty cards stored in the database. Returns `true` if the
  // operation succeeded.
  bool ClearLoyaltyCards();

 private:
  bool InitLoyaltyCardsTable();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PASSES_PASSES_TABLE_H_
