// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuables_table.h"

#include <optional>
#include <string_view>

#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/data_model/valuables/valuable_types.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/webdata/common/web_database.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace autofill {

namespace {

constexpr std::string_view kLoyaltyCardsTable = "loyalty_cards";
constexpr std::string_view kLoyaltyCardId = "loyalty_card_id";
constexpr std::string_view kLoyaltyCardMerchantName = "merchant_name";
constexpr std::string_view kLoyaltyCardProgramName = "program_name";
constexpr std::string_view kLoyaltyCardProgramLogo = "program_logo";
constexpr std::string_view kLoyaltyCardNumber = "loyalty_card_number";

// Expects that `s` is pointing to a query result containing `kLoyaltyCardId`,
// `kLoyaltyCardMerchantName`, `kLoyaltyCardProgramName`,
// `kLoyaltyCardProgramLogo` and `kUnmaskedLoyaltyCardSuffix` in that order.
// Constructs a `LoyaltyCard` from that data.
std::optional<LoyaltyCard> LoyaltyCardFromStatement(sql::Statement& s) {
  LoyaltyCard card(/*loyalty_card_id=*/ValuableId(s.ColumnString(0)),
                   /*merchant_name=*/s.ColumnString(1),
                   /*program_name=*/s.ColumnString(2),
                   /*program_logo=*/GURL(s.ColumnStringView(3)),
                   /*loyalty_card_number=*/s.ColumnString(4));
  // Ignore invalid loyalty cards, for more information see
  // `LoyaltyCard::IsValid()`. Loyalty cards coming from sync should be valid,
  // so this situation should not happen.
  return card.IsValid() ? std::optional(std::move(card)) : std::nullopt;
}

WebDatabaseTable::TypeKey GetKey() {
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

}  // namespace

ValuablesTable::ValuablesTable() = default;

ValuablesTable::~ValuablesTable() = default;

// static
ValuablesTable* ValuablesTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<ValuablesTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey ValuablesTable::GetTypeKey() const {
  return GetKey();
}

bool ValuablesTable::CreateTablesIfNecessary() {
  return InitLoyaltyCardsTable();
}

bool ValuablesTable::InitLoyaltyCardsTable() {
  return CreateTableIfNotExists(db(), kLoyaltyCardsTable,
                                {{kLoyaltyCardId, "TEXT PRIMARY KEY NOT NULL"},
                                 {kLoyaltyCardMerchantName, "TEXT NOT NULL"},
                                 {kLoyaltyCardProgramName, "TEXT NOT NULL"},
                                 {kLoyaltyCardProgramLogo, "TEXT NOT NULL"},
                                 {kLoyaltyCardNumber, "TEXT NOT NULL"}});
}

bool ValuablesTable::MigrateToVersion138() {
  // This is the legacy table name, which existed before and was renamed.
  const std::string kLoyaltyCardTable = "loyalty_card";

  sql::Transaction transaction(db());
  // The migration drops the table because no data is supposed to be stored in
  // it yet.
  return transaction.Begin() && DropTableIfExists(db(), kLoyaltyCardTable) &&
         DropTableIfExists(db(), kLoyaltyCardsTable) &&
         CreateTable(db(), kLoyaltyCardsTable,
                     {{kLoyaltyCardId, "TEXT PRIMARY KEY NOT NULL"},
                      {kLoyaltyCardMerchantName, "TEXT NOT NULL"},
                      {kLoyaltyCardProgramName, "TEXT NOT NULL"},
                      {kLoyaltyCardProgramLogo, "TEXT NOT NULL"},
                      {kLoyaltyCardNumber, "TEXT NOT NULL"}}) &&
         transaction.Commit();
}

bool ValuablesTable::MigrateToVersion(int version,
                                      bool* update_compatible_version) {
  switch (version) {
    case 138:
      *update_compatible_version = true;
      return MigrateToVersion138();
  }
  return true;
}

std::vector<LoyaltyCard> ValuablesTable::GetLoyaltyCards() const {
  sql::Statement query;
  SelectBuilder(
      db(), query, kLoyaltyCardsTable,
      {kLoyaltyCardId, kLoyaltyCardMerchantName, kLoyaltyCardProgramName,
       kLoyaltyCardProgramLogo, kLoyaltyCardNumber});
  std::vector<LoyaltyCard> result;
  while (query.Step()) {
    if (auto loyalty_card = LoyaltyCardFromStatement(query)) {
      result.emplace_back(std::move(*loyalty_card));
    }
  }
  return result;
}

bool ValuablesTable::AddOrUpdateLoyaltyCard(
    const LoyaltyCard& loyalty_card) const {
  if (!loyalty_card.IsValid()) {
    // Don't add loyalty cards with non-empty invalid program logo URLs.
    return false;
  }
  sql::Statement query;
  InsertBuilder(
      db(), query, kLoyaltyCardsTable,
      {kLoyaltyCardId, kLoyaltyCardMerchantName, kLoyaltyCardProgramName,
       kLoyaltyCardProgramLogo, kLoyaltyCardNumber},
      /*or_replace=*/true);
  int index = 0;
  query.BindString(index++, loyalty_card.id().value());
  query.BindString(index++, loyalty_card.merchant_name());
  query.BindString(index++, loyalty_card.program_name());
  query.BindString(index++, loyalty_card.program_logo().spec());
  query.BindString(index++, loyalty_card.loyalty_card_number());
  return query.Run();
}

std::optional<LoyaltyCard> ValuablesTable::GetLoyaltyCardById(
    ValuableId loyalty_card_id) const {
  sql::Statement query;
  SelectBuilder(
      db(), query, kLoyaltyCardsTable,
      {kLoyaltyCardId, kLoyaltyCardMerchantName, kLoyaltyCardProgramName,
       kLoyaltyCardProgramLogo, kLoyaltyCardNumber},
      "WHERE loyalty_card_id=?");
  query.BindString(0, loyalty_card_id.value());
  if (query.is_valid() && query.Step()) {
    return LoyaltyCardFromStatement(query);
  }
  return std::nullopt;
}

bool ValuablesTable::RemoveLoyaltyCard(ValuableId loyalty_card_id) {
  return DeleteWhereColumnEq(db(), kLoyaltyCardsTable, kLoyaltyCardId,
                             loyalty_card_id.value());
}

bool ValuablesTable::ClearLoyaltyCards() {
  return Delete(db(), kLoyaltyCardsTable);
}

}  // namespace autofill
