// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuables_table.h"

#include <cstdint>
#include <optional>
#include <string_view>

#include "base/check_deref.h"
#include "base/strings/strcat.h"
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

constexpr std::string_view kLoyaltyCardMerchantDomainTable =
    "loyalty_card_merchant_domain";
constexpr std::string_view kMerchantDomain = "merchant_domain";

constexpr std::string_view kValuableId = "valuable_id";
constexpr std::string_view kValuablesMetadataTable = "valuables_metadata";
constexpr std::string_view kUseCount = "use_count";
constexpr std::string_view kUseDate = "use_date";

// Returns the merchant domains for the loyalty card identified by
// `loyalty_card_id`.
std::vector<GURL> GetMerchantDomainsForLoyaltyCardId(
    sql::Database* db,
    const ValuableId& loyalty_card_id) {
  std::vector<GURL> merchant_domains;
  sql::Statement s_card_merchant_domain;
  SelectBuilder(db, s_card_merchant_domain, kLoyaltyCardMerchantDomainTable,
                {kLoyaltyCardId, kMerchantDomain},
                base::StrCat({"WHERE ", kLoyaltyCardId, " = ?"}));
  s_card_merchant_domain.BindString(0, loyalty_card_id.value());
  while (s_card_merchant_domain.Step()) {
    const std::string merchant_domain = s_card_merchant_domain.ColumnString(1);
    if (!merchant_domain.empty()) {
      merchant_domains.emplace_back(merchant_domain);
    }
  }
  return merchant_domains;
}

// Returns the metadata for the valuable identified by `valuable_id`.
std::optional<ValuableMetadata> GetValuableMetadataFromDb(
    sql::Database* db,
    const ValuableId& valuable_id) {
  sql::Statement s_valuable_metadata;
  SelectBuilder(db, s_valuable_metadata, kValuablesMetadataTable,
                {kValuableId, kUseCount, kUseDate},
                base::StrCat({"WHERE ", kValuableId, " = ?"}));
  s_valuable_metadata.BindString(0, valuable_id.value());

  if (!s_valuable_metadata.Step()) {
    return std::nullopt;
  }

  ValuableId valuable_id_(s_valuable_metadata.ColumnString(0));
  int64_t use_count = s_valuable_metadata.ColumnInt64(1);
  base::Time use_date = s_valuable_metadata.ColumnTime(2);

  return ValuableMetadata(valuable_id_, use_date, use_count);
}

// Expects that `s` is pointing to a query result containing `kLoyaltyCardId`,
// `kLoyaltyCardMerchantName`, `kLoyaltyCardProgramName`,
// `kLoyaltyCardProgramLogo` and `kLoyaltyCardNumber` in that order.
// Constructs a `LoyaltyCard` from that data.
std::optional<LoyaltyCard> LoyaltyCardFromStatement(sql::Database* db,
                                                    sql::Statement& s) {
  ValuableId loyalty_card_id = ValuableId(s.ColumnString(0));

  std::optional<ValuableMetadata> metadata =
      GetValuableMetadataFromDb(db, loyalty_card_id);

  const base::Time use_date = metadata ? metadata->use_date : base::Time();
  const int64_t use_count = metadata ? metadata->use_count : 0;

  LoyaltyCard card(
      /*loyalty_card_id=*/loyalty_card_id,
      /*merchant_name=*/s.ColumnString(1),
      /*program_name=*/s.ColumnString(2),
      /*program_logo=*/GURL(s.ColumnStringView(3)),
      /*loyalty_card_number=*/s.ColumnString(4),
      /*merchant_domains=*/
      GetMerchantDomainsForLoyaltyCardId(db, loyalty_card_id),
      /*use_date=*/use_date,
      /*use_count=*/use_count);

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
  return static_cast<ValuablesTable*>(CHECK_DEREF(db).GetTable(GetKey()));
}

WebDatabaseTable::TypeKey ValuablesTable::GetTypeKey() const {
  return GetKey();
}

bool ValuablesTable::CreateTablesIfNecessary() {
  return InitLoyaltyCardsTable() && InitLoyaltyCardMerchantDomainTable() &&
         InitValuablesMetadataTable();
}

bool ValuablesTable::InitLoyaltyCardsTable() {
  return CreateTableIfNotExists(db(), kLoyaltyCardsTable,
                                {{kLoyaltyCardId, "TEXT PRIMARY KEY NOT NULL"},
                                 {kLoyaltyCardMerchantName, "TEXT NOT NULL"},
                                 {kLoyaltyCardProgramName, "TEXT NOT NULL"},
                                 {kLoyaltyCardProgramLogo, "TEXT NOT NULL"},
                                 {kLoyaltyCardNumber, "TEXT NOT NULL"}});
}

bool ValuablesTable::InitLoyaltyCardMerchantDomainTable() {
  return CreateTableIfNotExists(
      db(), kLoyaltyCardMerchantDomainTable,
      {{kLoyaltyCardId, "VARCHAR"}, {kMerchantDomain, "VARCHAR"}});
}

bool ValuablesTable::InitValuablesMetadataTable() {
  return CreateTableIfNotExists(db(), kValuablesMetadataTable,
                                {{kValuableId, "TEXT PRIMARY KEY NOT NULL"},
                                 {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                                 {kUseDate, "INTEGER NOT NULL DEFAULT 0"}});
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

bool ValuablesTable::MigrateToVersion148AddMetadataTable() {
  return CreateTable(db(), kValuablesMetadataTable,
                     {{kValuableId, "TEXT PRIMARY KEY NOT NULL"},
                      {kUseCount, "INTEGER NOT NULL DEFAULT 0"},
                      {kUseDate, "INTEGER NOT NULL DEFAULT 0"}});
}

bool ValuablesTable::MigrateToVersion(int version,
                                      bool* update_compatible_version) {
  switch (version) {
    case 138:
      *update_compatible_version = true;
      return MigrateToVersion138();
    case 148:
      *update_compatible_version = false;
      return MigrateToVersion148AddMetadataTable();
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
    if (std::optional<LoyaltyCard> loyalty_card =
            LoyaltyCardFromStatement(db(), query)) {
      result.emplace_back(std::move(*loyalty_card));
    }
  }
  return result;
}

bool ValuablesTable::SetLoyaltyCards(
    const std::vector<LoyaltyCard>& loyalty_cards) const {
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }
  // Metadata must be deleted before the cards because the delete query depends
  // on the cards being present in the `kLoyaltyCardsTable`.
  // `kValuablesMetadataTable` is generic, so the whole table cannot be cleared.
  bool response = Delete(db(), kValuablesMetadataTable,
                         base::StrCat({kValuableId, " IN (SELECT ", kValuableId,
                                       " FROM ", kLoyaltyCardsTable, ")"}));

  // Remove the existing set of loyalty cards.
  response &= Delete(db(), kLoyaltyCardsTable);
  response &= Delete(db(), kLoyaltyCardMerchantDomainTable);

  sql::Statement insert_cards;
  InsertBuilder(
      db(), insert_cards, kLoyaltyCardsTable,
      {kLoyaltyCardId, kLoyaltyCardMerchantName, kLoyaltyCardProgramName,
       kLoyaltyCardProgramLogo, kLoyaltyCardNumber});

  for (const LoyaltyCard& loyalty_card : loyalty_cards) {
    if (!loyalty_card.IsValid()) {
      // Should never happen. Invalid entities are filtered out in
      // ValuableSyncBridge::IsEntityDataValid. Nevertheless, this case is
      // handled gracefully.
      response = false;
      continue;
    }
    int index = 0;
    insert_cards.BindString(index++, loyalty_card.id().value());
    insert_cards.BindString(index++, loyalty_card.merchant_name());
    insert_cards.BindString(index++, loyalty_card.program_name());
    insert_cards.BindString(index++, loyalty_card.program_logo().spec());
    insert_cards.BindString(index++, loyalty_card.loyalty_card_number());
    response &= insert_cards.Run();
    insert_cards.Reset(/*clear_bound_vars=*/true);

    for (const GURL& merchant_domain : loyalty_card.merchant_domains()) {
      // Insert new loyalty_card_merchant_domain values.
      sql::Statement insert_card_merchant_domains;
      InsertBuilder(db(), insert_card_merchant_domains,
                    kLoyaltyCardMerchantDomainTable,
                    {kLoyaltyCardId, kMerchantDomain}, /*or_replace=*/true);
      insert_card_merchant_domains.BindString(0, loyalty_card.id().value());
      insert_card_merchant_domains.BindString(1, merchant_domain.spec());
      response &= insert_card_merchant_domains.Run();
    }

    // Add the loyalty card's metadata. This is not a critical operation, so
    // execution proceeds even if it fails.
    AddValuableMetadata(loyalty_card.metadata());
  }

  transaction.Commit();
  return response;
}

std::optional<LoyaltyCard> ValuablesTable::GetLoyaltyCardById(
    ValuableId loyalty_card_id) const {
  sql::Statement query;
  SelectBuilder(
      db(), query, kLoyaltyCardsTable,
      {kLoyaltyCardId, kLoyaltyCardMerchantName, kLoyaltyCardProgramName,
       kLoyaltyCardProgramLogo, kLoyaltyCardNumber},
      base::StrCat({"WHERE ", kLoyaltyCardId, " = ?"}));
  query.BindString(0, loyalty_card_id.value());
  if (query.is_valid() && query.Step()) {
    return LoyaltyCardFromStatement(db(), query);
  }
  return std::nullopt;
}

bool ValuablesTable::AddOrUpdateLoyaltyCard(const LoyaltyCard& card) {
  if (!card.IsValid()) {
    return false;
  }

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement s;
  InsertBuilder(
      db(), s, kLoyaltyCardsTable,
      {kLoyaltyCardId, kLoyaltyCardMerchantName, kLoyaltyCardProgramName,
       kLoyaltyCardProgramLogo, kLoyaltyCardNumber},
      /*or_replace=*/true);

  int index = 0;
  s.BindString(index++, card.id().value());
  s.BindString(index++, card.merchant_name());
  s.BindString(index++, card.program_name());
  s.BindString(index++, card.program_logo().spec());
  s.BindString(index++, card.loyalty_card_number());

  if (!s.Run()) {
    return false;
  }

  // Remove old merchant domains for this card.
  if (!DeleteWhereColumnEq(db(), kLoyaltyCardMerchantDomainTable,
                           kLoyaltyCardId, card.id().value())) {
    return false;
  }

  // Insert new merchant domains.
  for (const GURL& merchant_domain : card.merchant_domains()) {
    sql::Statement insert_domain;
    InsertBuilder(db(), insert_domain, kLoyaltyCardMerchantDomainTable,
                  {kLoyaltyCardId, kMerchantDomain});
    insert_domain.BindString(0, card.id().value());
    insert_domain.BindString(1, merchant_domain.spec());
    if (!insert_domain.Run()) {
      return false;
    }
  }

  // Add the loyalty card's metadata. This is not a critical operation, so
  // execution proceeds even if it fails.
  AddValuableMetadata(card.metadata());

  return transaction.Commit();
}

bool ValuablesTable::RemoveLoyaltyCard(ValuableId loyalty_card_id) {
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         DeleteWhereColumnEq(db(), kLoyaltyCardsTable, kLoyaltyCardId,
                             loyalty_card_id.value()) &&
         RemoveValuableMetadata(loyalty_card_id) && transaction.Commit();
}

bool ValuablesTable::AddOrUpdateValuableMetadata(
    const ValuableMetadata& metadata) {
  sql::Transaction transaction(db());
  return transaction.Begin() && RemoveValuableMetadata(metadata.valuable_id) &&
         AddValuableMetadata(metadata) && transaction.Commit();
}

bool ValuablesTable::RemoveValuableMetadata(ValuableId valuable_id) {
  return DeleteWhereColumnEq(db(), kValuablesMetadataTable, kValuableId,
                             valuable_id.value());
}

std::optional<ValuableMetadata> ValuablesTable::GetValuableMetadata(
    ValuableId valuable_id) const {
  return GetValuableMetadataFromDb(db(), valuable_id);
}

absl::flat_hash_map<ValuableId, ValuableMetadata>
ValuablesTable::GetAllValuableMetadata() const {
  absl::flat_hash_map<ValuableId, ValuableMetadata> all_metadata;
  sql::Statement s;
  SelectBuilder(db(), s, kValuablesMetadataTable,
                {kValuableId, kUseCount, kUseDate});

  while (s.Step()) {
    ValuableId valuable_id = ValuableId(s.ColumnString(0));
    int64_t use_count = s.ColumnInt64(1);
    base::Time use_date = s.ColumnTime(2);
    all_metadata.emplace(valuable_id,
                         ValuableMetadata(valuable_id, use_date, use_count));
  }
  if (!s.Succeeded()) {
    return {};
  }
  return all_metadata;
}

bool ValuablesTable::AddValuableMetadata(
    const ValuableMetadata& metadata) const {
  sql::Statement s;
  InsertBuilder(db(), s, kValuablesMetadataTable,
                {kValuableId, kUseCount, kUseDate});
  int index = 0;
  s.BindString(index++, metadata.valuable_id.value());
  s.BindInt64(index++, metadata.use_count);
  s.BindTime(index++, metadata.use_date);
  return s.Run();
}

}  // namespace autofill
