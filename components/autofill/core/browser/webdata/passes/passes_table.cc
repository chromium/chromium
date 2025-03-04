// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/passes/passes_table.h"

#include <optional>
#include <string_view>

#include "components/autofill/core/browser/data_model/passes/loyalty_card.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/webdata/common/web_database.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace autofill {

namespace {

constexpr std::string_view kLoyaltyCardsTable = "loyalty_card";
constexpr std::string_view kLoyaltyCardGuid = "guid";
constexpr std::string_view kLoyaltyCardMerchantName = "merchant_name";
constexpr std::string_view kLoyaltyCardProgramName = "program_name";
constexpr std::string_view kLoyaltyCardProgramLogo = "program_logo";
constexpr std::string_view kUnmaskedLoyaltyCardSuffix =
    "unmasked_loyalty_card_suffix";

// Expects that `s` is pointing to a query result containing `kLoyaltyCardGuid`,
// `kLoyaltyCardMerchantName`, `kLoyaltyCardProgramName`,
// `kLoyaltyCardProgramLogo` and `kUnmaskedLoyaltyCardSuffix` in that order.
// Constructs a `LoyaltyCard` from that data.
std::optional<LoyaltyCard> LoyaltyCardFromStatement(sql::Statement& s) {
  LoyaltyCard card(/*loyalty_card_id=*/s.ColumnString(0),
                   /*merchant_name=*/s.ColumnString(1),
                   /*program_name=*/s.ColumnString(2),
                   /*program_logo=*/GURL(s.ColumnString(3)),
                   /*unmasked_loyalty_card_suffix=*/s.ColumnString(4));
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

PassesTable::PassesTable() = default;

PassesTable::~PassesTable() = default;

// static
PassesTable* PassesTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<PassesTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey PassesTable::GetTypeKey() const {
  return GetKey();
}

bool PassesTable::CreateTablesIfNecessary() {
  return InitLoyaltyCardsTable();
}

bool PassesTable::InitLoyaltyCardsTable() {
  return CreateTableIfNotExists(
      db(), kLoyaltyCardsTable,
      {{kLoyaltyCardGuid, "TEXT PRIMARY KEY NOT NULL"},
       {kLoyaltyCardMerchantName, "TEXT NOT NULL"},
       {kLoyaltyCardProgramName, "TEXT NOT NULL"},
       {kLoyaltyCardProgramLogo, "TEXT NOT NULL"},
       {kUnmaskedLoyaltyCardSuffix, "TEXT NOT NULL"}});
}

bool PassesTable::MigrateToVersion(int version,
                                   bool* update_compatible_version) {
  // No migrations exist at this point.
  return true;
}

std::vector<LoyaltyCard> PassesTable::GetLoyaltyCards() const {
  sql::Statement query;
  SelectBuilder(
      db(), query, kLoyaltyCardsTable,
      {kLoyaltyCardGuid, kLoyaltyCardMerchantName, kLoyaltyCardProgramName,
       kLoyaltyCardProgramLogo, kUnmaskedLoyaltyCardSuffix});
  std::vector<LoyaltyCard> result;
  while (query.Step()) {
    if (auto loyalty_card = LoyaltyCardFromStatement(query)) {
      result.emplace_back(std::move(*loyalty_card));
    }
  }
  return result;
}

bool PassesTable::AddOrUpdateLoyaltyCard(
    const LoyaltyCard& loyalty_card) const {
  if (!loyalty_card.IsValid()) {
    // Don't add loyalty cards with non-empty invalid program logo URLs.
    return false;
  }
  sql::Statement query;
  InsertBuilder(
      db(), query, kLoyaltyCardsTable,
      {kLoyaltyCardGuid, kLoyaltyCardMerchantName, kLoyaltyCardProgramName,
       kLoyaltyCardProgramLogo, kUnmaskedLoyaltyCardSuffix},
      /*or_replace=*/true);
  int index = 0;
  query.BindString(index++, loyalty_card.loyalty_card_id);
  query.BindString(index++, loyalty_card.merchant_name);
  query.BindString(index++, loyalty_card.program_name);
  query.BindString(index++, loyalty_card.program_logo.spec());
  query.BindString(index++, loyalty_card.unmasked_loyalty_card_suffix);
  return query.Run();
}

std::optional<LoyaltyCard> PassesTable::GetLoyaltyCardById(
    std::string_view loyalty_card_id) const {
  sql::Statement query;
  if (SelectByGuid(
          db(), query, kLoyaltyCardsTable,
          {kLoyaltyCardGuid, kLoyaltyCardMerchantName, kLoyaltyCardProgramName,
           kLoyaltyCardProgramLogo, kUnmaskedLoyaltyCardSuffix},
          loyalty_card_id)) {
    return LoyaltyCardFromStatement(query);
  }
  return std::nullopt;
}

bool PassesTable::RemoveLoyaltyCard(std::string_view loyalty_card_id) {
  return DeleteWhereColumnEq(db(), kLoyaltyCardsTable, kLoyaltyCardGuid,
                             loyalty_card_id);
}

bool PassesTable::ClearLoyaltyCards() {
  return Delete(db(), kLoyaltyCardsTable);
}

}  // namespace autofill
