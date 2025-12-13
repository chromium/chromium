// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLES_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLES_TABLE_H_

#include <optional>
#include <vector>

#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
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
//   loyalty_card_id      Uniquely identifies the loyalty card instance (primary
//                        key).
//   merchant_name        The name of the loyalty card provider e.g.
//                        "Deutsche Bahn".
//   program_name         The name of the program from the loyalty card provider
//                        e.g. "BahnBonus".
//   program_logo         The url of the logo icon for the card.
//   loyalty_card_number  A string representation of the unmasked loyalty card
//                        number suffix.
// -----------------------------------------------------------------------------
// loyalty_card_merchant_domain
//                      Contains the mapping of merchant domains and card linked
//                      offers.
//
//   loyalty_card_id    Identifies the relevant loyalty card. Matches the
//                      `loyalty_card_id` in the loyalty_cards table.
//   merchant_domain    List of full origins for merchant websites on which
//                      this card would apply.
// -----------------------------------------------------------------------------
class ValuablesTable : public WebDatabaseTable {
 public:
  ValuablesTable();

  ValuablesTable(const ValuablesTable&) = delete;
  ValuablesTable& operator=(const ValuablesTable&) = delete;

  ~ValuablesTable() override;

  // Retrieves the ValuablesTable* owned by `db`.
  static ValuablesTable* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Returns all loyalty cards stored in the database.
  std::vector<LoyaltyCard> GetLoyaltyCards() const;

  // Overwrites the existing set of loyalty cards in the DB with those in
  // `loyalty_cards`. Returns `true` if the database operation succeeded with no
  // errors.
  bool SetLoyaltyCards(const std::vector<LoyaltyCard>& loyalty_cards) const;

  // Attempts to retrieve a loyalty card from the database using the
  // `loyalty_card_id` as a unique identifier. Returns `std::nullopt` if there's
  // no loyalty card with `loyalty_card_id` in the database.
  std::optional<LoyaltyCard> GetLoyaltyCardById(
      ValuableId loyalty_card_id) const;

  // Adds or updates a loyalty card. Returns true on success.
  bool AddOrUpdateLoyaltyCard(const LoyaltyCard& card);

  // Removes the loyalty card from the database using `loyalty_card_id` as a
  // unique identifier. Returns `true` if the operation succeeded.
  bool RemoveLoyaltyCard(ValuableId loyalty_card_id);

 private:
  bool InitLoyaltyCardsTable();
  bool InitLoyaltyCardMerchantDomainTable();

  // Renames the database table from `loyalty_card` to `loyalty_cards` and
  // renames the following columns:
  //   * `guid` to `loyalty_card_id`.
  //   * `unmasked_loyalty_card_suffix` to `loyalty_card_number`.
  bool MigrateToVersion138();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLES_TABLE_H_
