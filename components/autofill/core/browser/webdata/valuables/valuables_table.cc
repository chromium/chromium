// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuables_table.h"

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

// Expects that `s` is pointing to a query result containing `kLoyaltyCardId`,
// `kLoyaltyCardMerchantName`, `kLoyaltyCardProgramName`,
// `kLoyaltyCardProgramLogo` and `kLoyaltyCardNumber` in that order.
// Constructs a `LoyaltyCard` from that data.
std::optional<LoyaltyCard> LoyaltyCardFromStatement(sql::Database* db,
                                                    sql::Statement& s) {
  ValuableId loyalty_card_id = ValuableId(s.ColumnString(0));
  LoyaltyCard card(
      /*loyalty_card_id=*/loyalty_card_id,
      /*merchant_name=*/s.ColumnString(1),
      /*program_name=*/s.ColumnString(2),
      /*program_logo=*/GURL(s.ColumnStringView(3)),
      /*loyalty_card_number=*/s.ColumnString(4),
      /*merchant_domains=*/
      GetMerchantDomainsForLoyaltyCardId(db, loyalty_card_id));
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
  return InitLoyaltyCardsTable() && InitLoyaltyCardMerchantDomainTable();
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
  // Remove the existing set of loyalty cards.
  bool response = Delete(db(), kLoyaltyCardsTable);
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

bool ValuablesTable::RemoveLoyaltyCard(ValuableId loyalty_card_id) {
  return DeleteWhereColumnEq(db(), kLoyaltyCardsTable, kLoyaltyCardId,
                             loyalty_card_id.value());
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

  return transaction.Commit();
}

}  // namespace autofill
