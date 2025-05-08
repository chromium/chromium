// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"

#include <algorithm>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/field_types.h"
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

namespace attributes {
constexpr char kTableName[] = "autofill_ai_attributes";
constexpr char kEntityGuid[] = "entity_guid";
constexpr char kAttributeType[] = "attribute_type";
constexpr char kFieldType[] = "field_type";
constexpr char kValueEncrypted[] = "value_encrypted";
constexpr char kVerificationStatus[] = "verification_status";
}  // namespace attributes

namespace entities {
constexpr char kTableName[] = "autofill_ai_entities";
constexpr char kGuid[] = "guid";
constexpr char kEntityType[] = "entity_type";
constexpr char kNickname[] = "nickname";
constexpr char kDateModified[] = "date_modified";
constexpr char kUseCount[] = "use_count";
constexpr char kUseDate[] = "use_date";
}  // namespace entities

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
    auto create_attribute = [](AttributeTypeName type_name,
                               std::u16string value) -> AttributeInstance {
      auto type = AttributeType(type_name);
      auto instance = AttributeInstance(AttributeType(type));
      instance.SetInfo(instance.type().field_type(), value, /*app_locale=*/"",
                       /*format_string=*/
                       IsDateFieldType(type.field_type()) ? u"YYYY-MM-DD" : u"",
                       VerificationStatus::kNoStatus);
      return instance;
    };

    using enum AttributeTypeName;

    table.AddOrUpdateEntityInstance(EntityInstance(
        EntityType(EntityTypeName::kPassport),
        {create_attribute(kPassportNumber, u"123"),
         create_attribute(kPassportName, u"Pippi Långstrump"),
         create_attribute(kPassportCountry, u"Sweden"),
         create_attribute(kPassportExpirationDate, u"2035-03-31"),
         create_attribute(kPassportIssueDate, u"1998-10-11")},
        base::Uuid::ParseLowercase("00000000-0000-4000-8000-123000000000"),
        "My passport", /*date_modified=*/base::Time::Now(), /*use_count=*/0,
        /*use_date=*/base::Time::FromTimeT(0)));

    table.AddOrUpdateEntityInstance(EntityInstance(
        EntityType(EntityTypeName::kDriversLicense),
        {create_attribute(kDriversLicenseNumber, u"456"),
         create_attribute(kDriversLicenseName, u"Jim Hacker"),
         create_attribute(kDriversLicenseState, u"California"),
         create_attribute(kDriversLicenseExpirationDate, u"2069-12-31"),
         create_attribute(kDriversLicenseIssueDate, u"1969-12-24")},
        base::Uuid::ParseLowercase("00000000-0000-4000-8000-456000000000"),
        "My license", /*date_modified=*/base::Time::Now(), /*use_count=*/0,
        /*use_date=*/base::Time::FromTimeT(0)));

    table.AddOrUpdateEntityInstance(EntityInstance(
        EntityType(EntityTypeName::kVehicle),
        {create_attribute(kVehicleMake, u"BMW"),
         create_attribute(kVehicleModel, u"3 series"),
         create_attribute(kVehicleYear, u"2024"),
         create_attribute(kVehicleOwner, u"Humphrey Appleby"),
         create_attribute(kVehiclePlateNumber, u"SUNNY1133"),
         create_attribute(kVehiclePlateState, u"California"),
         create_attribute(kVehicleVin, u"3D73Y4CL2AG194665")},
        base::Uuid::ParseLowercase("00000000-0000-4000-8000-789000000000"),
        "My wroom wroom car", /*date_modified=*/base::Time::Now(),
        /*use_count=*/0, /*use_date=*/base::Time::FromTimeT(0)));
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
  auto create_attributes_table = [&] {
    return CreateTableIfNotExists(
        db(), /*table_name=*/attributes::kTableName,
        /*column_names_and_types=*/
        {{attributes::kEntityGuid, "TEXT NOT NULL"},
         {attributes::kAttributeType, "TEXT NOT NULL"},
         {attributes::kFieldType, "INTEGER NOT NULL"},
         {attributes::kValueEncrypted, "BLOB NOT NULL"},
         {attributes::kVerificationStatus, "INTEGER NOT NULL"}},
        /*composite_primary_key=*/
        {attributes::kEntityGuid, attributes::kAttributeType,
         attributes::kFieldType});
  };
  auto create_entities_table = [&] {
    return CreateTableIfNotExists(
        db(), /*table_name=*/entities::kTableName,
        /*column_names_and_types=*/
        {{entities::kGuid, "TEXT NOT NULL PRIMARY KEY"},
         {entities::kEntityType, "TEXT NOT NULL"},
         {entities::kNickname, "TEXT NOT NULL"},
         {entities::kDateModified, "INTEGER NOT NULL"},
         {entities::kUseCount, "INTEGER DEFAULT 0"},
         {entities::kUseDate, "INTEGER DEFAULT 0"}});
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
    case 138: {
      // Up to version 138, AutofillAi was purely experimental. To upgrade from
      // earlier versions, we can simply drop all previous data.
      DropTableIfExists(db(), "attributes");
      DropTableIfExists(db(), "entities");
      DropTableIfExists(db(), "entities_version");
      CreateTableIfNotExists(db(), /*table_name=*/"autofill_ai_attributes",
                             /*column_names_and_types=*/
                             {{"entity_guid", "TEXT NOT NULL"},
                              {"attribute_type", "TEXT NOT NULL"},
                              {"field_type", "INTEGER NOT NULL"},
                              {"value_encrypted", "BLOB NOT NULL"},
                              {"verification_status", "INTEGER NOT NULL"}},
                             /*composite_primary_key=*/
                             {"entity_guid", "attribute_type", "field_type"});
      CreateTableIfNotExists(db(), /*table_name=*/"autofill_ai_entities",
                             /*column_names_and_types=*/
                             {{"guid", "TEXT NOT NULL PRIMARY KEY"},
                              {"entity_type", "TEXT NOT NULL"},
                              {"nickname", "TEXT NOT NULL"},
                              {"date_modified", "INTEGER NOT NULL"}});

      *update_compatible_version = true;
      break;
    }
    case 140: {
      // In this version use count and use date information was added.
      AddColumn(db(), "autofill_ai_entities", "use_count", "INTEGER DEFAULT 0");
      AddColumn(db(), "autofill_ai_entities", "use_date", "INTEGER DEFAULT 0");
      break;
    }
  }
  return true;
}

bool EntityTable::AddAttribute(const EntityInstance& entity,
                               const AttributeInstance& attribute) {
  for (FieldType type : attribute.GetDatabaseStoredTypes()) {
    sql::Statement s;
    InsertBuilder(db(), s, attributes::kTableName,
                  {attributes::kEntityGuid, attributes::kAttributeType,
                   attributes::kFieldType, attributes::kValueEncrypted,
                   attributes::kVerificationStatus});
    s.BindString(0, entity.guid().AsLowercaseString());
    s.BindString(1, attribute.type().name_as_string());
    s.BindInt(2, type);
    if (std::string encrypted_value; encryptor()->EncryptString16(
            attribute.GetRawInfo(/*pass_key=*/{}, type), &encrypted_value)) {
      s.BindString(3, encrypted_value);
    } else {
      return false;
    }
    s.BindInt(4, static_cast<int>(attribute.GetVerificationStatus(type)));
    if (!s.Run()) {
      return false;
    }
  }
  return true;
}

bool EntityTable::AddEntityInstance(const EntityInstance& entity) {
  HandleTestSwitchesIfNeeded(db(), *this);

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }
  // Add the attributes. In case of failure for any attribute, do not add the
  // entity.
  if (!std::ranges::all_of(entity.attributes(),
                           [&](const AttributeInstance& attribute) {
                             return AddAttribute(entity, attribute);
                           })) {
    return false;
  }

  // Add the entity.
  sql::Statement s;
  InsertBuilder(
      db(), s, entities::kTableName,
      {entities::kGuid, entities::kEntityType, entities::kNickname,
       entities::kDateModified, entities::kUseCount, entities::kUseDate});
  s.BindString(0, entity.guid().AsLowercaseString());
  s.BindString(1, entity.type().name_as_string());
  s.BindString(2, entity.nickname());
  s.BindInt64(3, entity.date_modified().ToTimeT());
  s.BindInt64(4, entity.use_count());
  s.BindTime(5, entity.use_date());

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
    base::Uuid guid = base::Uuid::ParseLowercase(s.ColumnStringView(0));
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

std::map<base::Uuid,
         std::map<std::string, std::vector<EntityTable::AttributeRecord>>>
EntityTable::LoadAttributes() const {
  std::map<base::Uuid, std::map<std::string, std::vector<AttributeRecord>>>
      attribute_records;
  sql::Statement s;
  SelectBuilder(db(), s, attributes::kTableName,
                {attributes::kEntityGuid, attributes::kAttributeType,
                 attributes::kFieldType, attributes::kValueEncrypted,
                 attributes::kVerificationStatus});
  while (s.Step()) {
    base::Uuid entity_guid = base::Uuid::ParseLowercase(s.ColumnStringView(0));
    std::string attribute_type_name = s.ColumnString(1);
    std::underlying_type_t<FieldType> underlying_field_type = s.ColumnInt(2);
    std::u16string decrypted_value;
    if (!encryptor()->DecryptString16(s.ColumnString(3), &decrypted_value)) {
      continue;
    }
    std::underlying_type_t<VerificationStatus> underlying_verification_status =
        s.ColumnInt(4);
    attribute_records[entity_guid][attribute_type_name].push_back(
        {.field_type = underlying_field_type,
         .value = decrypted_value,
         .verification_status = underlying_verification_status});
  }
  if (!s.Succeeded()) {
    return {};
  }
  return attribute_records;
}

std::vector<EntityInstance> EntityTable::GetEntityInstances() const {
  HandleTestSwitchesIfNeeded(db(), const_cast<EntityTable&>(*this));

  // Collects all attributes, keyed by the owning entity's GUID and the
  // `AttributeTypeName` of the attribute.
  std::map<base::Uuid,
           std::map<std::string, std::vector<EntityTable::AttributeRecord>>>
      attribute_records = LoadAttributes();

  // Collects all entities and populates them with the attributes from the
  // previous query.
  std::vector<EntityInstance> entities;
  sql::Statement s;
  SelectBuilder(
      db(), s, entities::kTableName,
      {entities::kGuid, entities::kEntityType, entities::kNickname,
       entities::kDateModified, entities::kUseCount, entities::kUseDate});
  while (s.Step()) {
    base::Uuid guid = base::Uuid::ParseLowercase(s.ColumnStringView(0));
    std::string type_name = s.ColumnString(1);
    std::string nickname = s.ColumnString(2);
    base::Time date_modified = base::Time::FromTimeT(s.ColumnInt64(3));
    size_t use_count = s.ColumnInt64(4);
    base::Time use_date = s.ColumnTime(5);

    if (auto attributes = attribute_records.extract(guid)) {
      if (std::optional<EntityInstance> e = ValidateInstance(
              type_name, std::move(guid), std::move(nickname), date_modified,
              use_count, use_date, std::move(attributes.mapped()))) {
        entities.push_back(*std::move(e));
      }
    }
  }
  if (!s.Succeeded()) {
    return {};
  }
  return entities;
}

std::optional<EntityInstance> EntityTable::ValidateInstance(
    std::string_view type_name,
    base::Uuid guid,
    std::string nickname,
    base::Time date_modified,
    int use_count,
    base::Time use_date,
    std::map<std::string, std::vector<AttributeRecord>> attribute_records)
    const {
  std::optional<EntityType> entity_type =
      StringToEntityType(/*pass_key=*/{}, type_name);
  if (!entity_type || !guid.is_valid()) {
    return std::nullopt;
  }

  std::vector<AttributeInstance> attributes;

  for (const auto& [attribute_type_name, records] : attribute_records) {
    if (std::optional<AttributeType> attribute_type = StringToAttributeType(
            /*pass_key=*/{}, *entity_type, attribute_type_name)) {
      AttributeInstance& attribute = attributes.emplace_back(*attribute_type);
      for (const auto& [underlying_field_type, value,
                        underlying_verification_status] : records) {
        FieldType field_type =
            ToSafeFieldType(underlying_field_type, UNKNOWN_TYPE);
        std::optional<VerificationStatus> verification_status =
            ToSafeVerificationStatus(underlying_verification_status);
        if (field_type != UNKNOWN_TYPE && verification_status) {
          attribute.SetRawInfo(field_type, value, *verification_status);
        }
      }
    }
  }

  for (AttributeInstance& attribute : attributes) {
    attribute.FinalizeInfo();
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
                        std::move(nickname), date_modified, use_count,
                        use_date);
}

}  // namespace autofill
