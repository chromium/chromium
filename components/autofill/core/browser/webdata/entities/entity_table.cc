// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/entities/entity_table.h"

#include <map>
#include <optional>
#include <ranges>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace autofill {

namespace {

void* GetKey() {
  static char key = 0;
  return reinterpret_cast<void*>(&key);
}

// TODO(crbug.com/394292801): Remove when we migrate to WebDatabase's
// versioning.
namespace version {
constexpr char kTableName[] = "entities_version";
constexpr char kVersion[] = "version";
constexpr int kCurrentVersion = 3;
}  // namespace version

namespace attributes {
constexpr char kTableName[] = "attributes";
constexpr char kEntityGuid[] = "entity_guid";
constexpr char kType[] = "type";
constexpr char kValueEncrypted[] = "value_encrypted";
constexpr char kContext[] = "context";
}  // namespace attributes

namespace entities {
constexpr char kTableName[] = "entities";
constexpr char kGuid[] = "guid";
constexpr char kType[] = "type";
constexpr char kNickname[] = "nickname";
constexpr char kDateModified[] = "date_modified";
}  // namespace entities

struct AttributeRecord {
  std::string type_name;
  std::u16string value;
  AttributeInstance::Context context;
};

std::optional<EntityInstance> ValidateInstance(
    base::PassKey<EntityTable> pass_key,
    std::string_view type_name,
    std::vector<AttributeRecord> attribute_records,
    base::Uuid guid,
    std::string nickname,
    base::Time date_modified) {
  std::optional<EntityType> entity_type =
      StringToEntityType(pass_key, type_name);
  if (!entity_type || !guid.is_valid()) {
    return std::nullopt;
  }

  std::vector<AttributeInstance> attributes;
  attributes.reserve(attribute_records.size());
  for (AttributeRecord& ar : attribute_records) {
    if (std::optional<AttributeType> attribute_type =
            StringToAttributeType(pass_key, *entity_type, ar.type_name)) {
      attributes.emplace_back(*attribute_type, std::move(ar.value),
                              std::move(ar.context));
    }
  }

  // Remove attributes that don't belong to the entity according to the schema.
  // (The schema may have changed and this attribute may be outdated.)
  std::erase_if(attributes, [&entity_type](const AttributeInstance& a) {
    return *entity_type != a.type().entity_type();
  });

  if (attributes.empty()) {
    return std::nullopt;
  }

  return EntityInstance(*entity_type, std::move(attributes), std::move(guid),
                        std::move(nickname), date_modified);
}

// If "--autofill-wipe-entities" is present, drops the tables and creates
// new ones.
//
// If "--autofill-add-test-entities" is present, adds two example entities.
//
// TODO(crbug.com/388590912): Remove when test data is no longer needed.
void HandleTestSwitchesIfNeeded(sql::Database* db, EntityTable& table) {
  const bool wipe = base::CommandLine::ForCurrentProcess()->HasSwitch(
      "autofill-wipe-entities");
  const bool add = base::CommandLine::ForCurrentProcess()->HasSwitch(
      "autofill-add-test-entities");
  if (!wipe && !add) {
    return;
  }

  // Handle the switches only once.
  static bool has_been_called = false;
  if (has_been_called) {
    return;
  }
  has_been_called = true;

  if (wipe) {
    DropTableIfExists(db, attributes::kTableName);
    DropTableIfExists(db, entities::kTableName);
    table.CreateTablesIfNecessary();
  }

  if (add) {
    using enum AttributeTypeName;
    table.AddOrUpdateEntityInstance(EntityInstance(
        EntityType(EntityTypeName::kPassport),
        {AttributeInstance(AttributeType(kPassportNumber), u"123", {}),
         AttributeInstance(AttributeType(kPassportName), u"Pippi Långstrump",
                           {}),
         AttributeInstance(AttributeType(kPassportCountry), u"Sweden", {}),
         AttributeInstance(AttributeType(kPassportExpiryDate), u"09/2098", {}),
         AttributeInstance(AttributeType(kPassportIssueDate), u"10/1998", {})},
        base::Uuid::ParseLowercase("00000000-0000-4000-8000-000000000000"),
        "Passie", base::Time::Now()));
    table.AddOrUpdateEntityInstance(EntityInstance(
        EntityType(EntityTypeName::kLoyaltyCard),
        {AttributeInstance(AttributeType(kLoyaltyCardProgram),
                           u"Asterisk Alliance", {}),
         AttributeInstance(AttributeType(kLoyaltyCardProvider),
                           u"Propeller Airways", {}),
         AttributeInstance(AttributeType(kLoyaltyCardMemberId), u"987", {})},
        base::Uuid::ParseLowercase("11111111-1111-4111-8111-111111111111"),
        "Loyie", base::Time::Now()));
  }
}

}  // namespace

EntityTable::EntityTable() = default;
EntityTable::~EntityTable() = default;

// static
EntityTable* EntityTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<EntityTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey EntityTable::GetTypeKey() const {
  return GetKey();
}

bool EntityTable::CreateTablesIfNecessary() {
  // TODO(crbug.com/394292801): Remove when we migrate to WebDatabase's
  // versioning.
  {
    CreateTableIfNotExists(db(), /*table_name=*/version::kTableName,
                           /*column_names_and_types=*/
                           {{version::kVersion, "INTEGER"}});
    auto get_table_version = [&] {
      sql::Statement s;
      SelectBuilder(db(), s, version::kTableName, {version::kVersion});
      if (s.Step()) {
        return s.ColumnInt(0);
      }
      constexpr int kDefaultVersion = 0;
      InsertBuilder(db(), s, version::kTableName, {version::kVersion});
      s.BindInt(0, kDefaultVersion);
      s.Run();
      return kDefaultVersion;
    };
    if (get_table_version() != version::kCurrentVersion) {
      sql::Statement s;
      UpdateBuilder(db(), s, version::kTableName, {version::kVersion},
                    /*where_clause=*/"");
      s.BindInt(0, version::kCurrentVersion);
      s.Run();
      DropTableIfExists(db(), attributes::kTableName);
      DropTableIfExists(db(), entities::kTableName);
    }
  }

  auto create_attributes_table = [&] {
    return CreateTableIfNotExists(
        db(), /*table_name=*/attributes::kTableName,
        /*column_names_and_types=*/
        {{attributes::kEntityGuid, "TEXT NOT NULL"},
         {attributes::kType, "TEXT NOT NULL"},
         {attributes::kValueEncrypted, "BLOB NOT NULL"},
         {attributes::kContext, "TEXT"}},
        /*composite_primary_key=*/{attributes::kEntityGuid, attributes::kType});
  };
  auto create_entities_table = [&] {
    return CreateTableIfNotExists(
        db(), /*table_name=*/entities::kTableName,
        /*column_names_and_types=*/
        {{entities::kGuid, "TEXT NOT NULL PRIMARY KEY"},
         {entities::kType, "TEXT NOT NULL"},
         {entities::kNickname, "TEXT NOT NULL"},
         {entities::kDateModified, "INTEGER NOT NULL"}});
  };
  return create_attributes_table() && create_entities_table();
}

// There are two types of migration:
// 1. When the database schema changes (e.g., a column is added or deleted).
// 2. When the entity schema changes (e.g., an attribute is added or deleted).
//
// Type 1 migration can usually be handled with the functions from
// autofill_table_utils.h (e.g., AddColumn() or DropColumn()).
//
// Type 2 migration may need to migrate the database's tuples. This can follow
// the pattern
//   for (const EntityInstance& old_e : GetEntityInstances()) {
//     EntityInstance new_ = migrate(old_e);
//     AddOrUpdateEntityInstance(new_e);
//   }
// where migrate() maps the old to a new EntityInstance. To delete attributes,
// the identity function suffices because GetEntityInstances() skips unknown
// attributes.
bool EntityTable::MigrateToVersion(int version,
                                   bool* update_compatible_version) {
  switch (version) {
    // No migrations exist at this point.
  }
  return true;
}

bool EntityTable::AddEntityInstance(const EntityInstance& entity) {
  HandleTestSwitchesIfNeeded(db(), *this);

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }
  // Add the attributes.
  for (const AttributeInstance& attribute : entity.attributes()) {
    sql::Statement s;
    InsertBuilder(db(), s, attributes::kTableName,
                  {attributes::kEntityGuid, attributes::kType,
                   attributes::kValueEncrypted, attributes::kContext});
    s.BindString(0, entity.guid().AsLowercaseString());
    s.BindString(1, attribute.type().name_as_string());
    std::string encrypted_value;
    if (encryptor()->EncryptString16(attribute.value(), &encrypted_value)) {
      s.BindString(2, encrypted_value);
    } else {
      return false;
    }
    s.BindString(3, attribute.context().format);
    if (!s.Run()) {
      return false;
    }
  }

  // Add the entity.
  sql::Statement s;
  InsertBuilder(db(), s, entities::kTableName,
                {entities::kGuid, entities::kType, entities::kNickname,
                 entities::kDateModified});
  s.BindString(0, entity.guid().AsLowercaseString());
  s.BindString(1, entity.type().name_as_string());
  s.BindString(2, entity.nickname());
  s.BindInt64(3, entity.date_modified().ToTimeT());
  if (!s.Run()) {
    return false;
  }
  return transaction.Commit();
}

bool EntityTable::AddOrUpdateEntityInstance(const EntityInstance& entity) {
  HandleTestSwitchesIfNeeded(db(), *this);

  sql::Transaction transaction(db());
  return transaction.Begin() && RemoveEntityInstance(entity.guid()) &&
         AddEntityInstance(entity) && transaction.Commit();
}

bool EntityTable::RemoveEntityInstance(const base::Uuid& guid) {
  HandleTestSwitchesIfNeeded(db(), *this);

  sql::Transaction transaction(db());
  return transaction.Begin() &&
         DeleteWhereColumnEq(db(), attributes::kTableName,
                             attributes::kEntityGuid,
                             guid.AsLowercaseString()) &&
         DeleteWhereColumnEq(db(), entities::kTableName, entities::kGuid,
                             guid.AsLowercaseString()) &&
         transaction.Commit();
}

bool EntityTable::RemoveEntityInstancesModifiedBetween(base::Time delete_begin,
                                                       base::Time delete_end) {
  HandleTestSwitchesIfNeeded(db(), *this);

  if (delete_begin.is_null()) {
    delete_begin = base::Time::Min();
  }
  if (delete_end.is_null()) {
    delete_end = base::Time::Max();
  }

  sql::Statement s;
  SelectBuilder(db(), s, entities::kTableName, {entities::kGuid},
                "WHERE date_modified >= ? AND date_modified < ?");
  s.BindInt64(0, delete_begin.ToTimeT());
  s.BindInt64(1, delete_end.ToTimeT());
  std::vector<base::Uuid> guids;
  while (s.Step()) {
    base::Uuid guid = base::Uuid::ParseLowercase(s.ColumnString(0));
    if (!guid.is_valid()) {
      continue;
    }
    guids.push_back(std::move(guid));
  }
  if (!s.Succeeded()) {
    return false;
  }

  sql::Transaction transaction(db());
  return transaction.Begin() &&
         std::ranges::all_of(guids,
                             [this](const base::Uuid& guid) {
                               return RemoveEntityInstance(guid);
                             }) &&
         transaction.Commit();
}

std::vector<EntityInstance> EntityTable::GetEntityInstances() const {
  HandleTestSwitchesIfNeeded(db(), const_cast<EntityTable&>(*this));

  // Collects all attributes, keyed by the owning entity's GUID.
  std::map<base::Uuid, std::vector<AttributeRecord>> attribute_records;
  {
    sql::Statement s;
    SelectBuilder(db(), s, attributes::kTableName,
                  {attributes::kEntityGuid, attributes::kType,
                   attributes::kValueEncrypted, attributes::kContext});
    while (s.Step()) {
      base::Uuid entity_guid = base::Uuid::ParseLowercase(s.ColumnString(0));
      std::string type_name = s.ColumnString(1);
      std::u16string decrypted_value;
      if (!encryptor()->DecryptString16(s.ColumnString(2), &decrypted_value)) {
        continue;
      }
      AttributeInstance::Context context;
      context.format = s.ColumnString(3);
      attribute_records[entity_guid].push_back(
          {.type_name = std::move(type_name),
           .value = std::move(decrypted_value),
           .context = std::move(context)});
    }
    if (!s.Succeeded()) {
      return {};
    }
  }

  // Collects all entities and populates them with the attributes from the
  // previous query.
  std::vector<EntityInstance> entities;
  {
    sql::Statement s;
    SelectBuilder(db(), s, entities::kTableName,
                  {entities::kGuid, entities::kType, entities::kNickname,
                   entities::kDateModified});
    while (s.Step()) {
      base::Uuid guid = base::Uuid::ParseLowercase(s.ColumnString(0));
      std::string type_name = s.ColumnString(1);
      std::string nickname = s.ColumnString(2);
      base::Time date_modified = base::Time::FromTimeT(s.ColumnInt64(3));

      if (auto attributes = attribute_records.extract(guid)) {
        if (std::optional<EntityInstance> e = ValidateInstance(
                /*pass_key=*/{}, type_name, std::move(attributes.mapped()),
                std::move(guid), std::move(nickname), date_modified)) {
          entities.push_back(*std::move(e));
        }
      }
    }
    if (!s.Succeeded()) {
      return {};
    }
  }
  return entities;
}

}  // namespace autofill
