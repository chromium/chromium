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
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/server.pb.h"
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
constexpr char kRecordType[] = "record_type";
constexpr char kAttributesReadOnly[] = "attributes_read_only";
constexpr char kFrecencyOverride[] = "frecency_override";
}  // namespace entities

namespace entities_metadata {
constexpr char kTableName[] = "autofill_ai_entities_metadata";
constexpr char kEntityGuid[] = "entity_guid";
constexpr char kUseCount[] = "use_count";
constexpr char kUseDate[] = "use_date";
constexpr char kDateModified[] = "date_modified";
}  // namespace entities_metadata

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
    DropTableIfExists(db, autofill::attributes::kTableName);
    DropTableIfExists(db, autofill::entities::kTableName);
    DropTableIfExists(db, autofill::entities_metadata::kTableName);
    table.CreateTablesIfNecessary();
  }

  if (add) {
    auto create_attribute = [](AttributeTypeName type_name,
                               std::u16string value) -> AttributeInstance {
      auto type = AttributeType(type_name);
      auto instance = AttributeInstance(AttributeType(type));
      instance.SetInfo(
          instance.type().field_type(), value, /*app_locale=*/"",
          /*format_string=*/
          IsDateFieldType(type.field_type())
              ? AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE)
              : base::optional_ref<const AutofillFormatString>(),
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
        EntityInstance::EntityId(
            base::Uuid::ParseLowercase("00000000-0000-4000-8000-123000000000")),
        "My passport", /*date_modified=*/base::Time::Now(), /*use_count=*/0,
        /*use_date=*/base::Time::FromTimeT(0),
        EntityInstance::RecordType::kLocal,
        EntityInstance::AreAttributesReadOnly(false),
        /*frecency_override=*/""));

    table.AddOrUpdateEntityInstance(EntityInstance(
        EntityType(EntityTypeName::kDriversLicense),
        {create_attribute(kDriversLicenseNumber, u"456"),
         create_attribute(kDriversLicenseName, u"Jim Hacker"),
         create_attribute(kDriversLicenseState, u"California"),
         create_attribute(kDriversLicenseExpirationDate, u"2069-12-31"),
         create_attribute(kDriversLicenseIssueDate, u"1969-12-24")},
        EntityInstance::EntityId(
            base::Uuid::ParseLowercase("00000000-0000-4000-8000-456000000000")),
        "My license", /*date_modified=*/base::Time::Now(), /*use_count=*/0,
        /*use_date=*/base::Time::FromTimeT(0),
        EntityInstance::RecordType::kLocal,
        EntityInstance::AreAttributesReadOnly(false),
        /*frecency_override=*/""));

    table.AddOrUpdateEntityInstance(EntityInstance(
        EntityType(EntityTypeName::kVehicle),
        {create_attribute(kVehicleMake, u"BMW"),
         create_attribute(kVehicleModel, u"3 series"),
         create_attribute(kVehicleYear, u"2024"),
         create_attribute(kVehicleOwner, u"Humphrey Appleby"),
         create_attribute(kVehiclePlateNumber, u"SUNNY1133"),
         create_attribute(kVehiclePlateState, u"California"),
         create_attribute(kVehicleVin, u"3D73Y4CL2AG194665")},
        EntityInstance::EntityId(
            base::Uuid::ParseLowercase("00000000-0000-4000-8000-789000000000")),
        "My wroom wroom car", /*date_modified=*/base::Time::Now(),
        /*use_count=*/0, /*use_date=*/base::Time::FromTimeT(0),
        EntityInstance::RecordType::kLocal,
        EntityInstance::AreAttributesReadOnly(false),
        /*frecency_override=*/""));
  }
}

std::optional<EntityInstance::RecordType> ToSafeRecordType(
    std::underlying_type_t<EntityInstance::RecordType> underlying_record_type) {
  switch (EntityInstance::RecordType record_type =
              static_cast<EntityInstance::RecordType>(underlying_record_type)) {
    case EntityInstance::RecordType::kLocal:
    case EntityInstance::RecordType::kServerWallet:
      return record_type;
  }
  return std::nullopt;
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
         // TODO(crbug.com/450685388): Make all columns not null.
         {entities::kRecordType, "INTEGER DEFAULT 0"},
         {entities::kAttributesReadOnly, "INTEGER DEFAULT 0"},
         {entities::kFrecencyOverride, "TEXT NOT NULL DEFAULT ''"}});
  };
  auto create_entities_metadata_table = [&] {
    return CreateTableIfNotExists(
        db(), /*table_name=*/autofill::entities_metadata::kTableName,
        /*column_names_and_types=*/
        {{autofill::entities_metadata::kEntityGuid,
          "TEXT NOT NULL PRIMARY KEY"},
         // TODO(crbug.com/450685388): Make all columns not null.
         {autofill::entities_metadata::kUseCount, "INTEGER DEFAULT 0"},
         {autofill::entities_metadata::kUseDate, "INTEGER DEFAULT 0"},
         {autofill::entities_metadata::kDateModified, "INTEGER NOT NULL"}});
  };
  return create_attributes_table() && create_entities_table() &&
         create_entities_metadata_table();
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
    case 142: {
      // In this version the record type was added.
      return AddColumn(db(), "autofill_ai_entities", "record_type",
                       "INTEGER DEFAULT 0");
    }
    case 143: {
      // In this version `attributes_read_only` flag was added.
      return AddColumn(db(), "autofill_ai_entities", "attributes_read_only",
                       "INTEGER DEFAULT 0");
    }
    case 146: {
      // In this version `frecency_override` was added.
      return AddColumn(db(), "autofill_ai_entities", "frecency_override",
                       "TEXT NOT NULL DEFAULT ''");
    }
    case 147: {
      *update_compatible_version = true;
      return MigrateToVersion147AddEntitiesMetadataTable();
    }
  }
  return true;
}

bool EntityTable::MigrateToVersion147AddEntitiesMetadataTable() {
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         CreateTableIfNotExists(
             db(), /*table_name=*/entities_metadata::kTableName,
             /*column_names_and_types=*/
             {{entities_metadata::kEntityGuid, "TEXT NOT NULL PRIMARY KEY"},
              {entities_metadata::kUseCount, "INTEGER DEFAULT 0"},
              {entities_metadata::kUseDate, "INTEGER DEFAULT 0"},
              {entities_metadata::kDateModified, "INTEGER NOT NULL"}}) &&
         db()->Execute(base::StrCat(
             {"INSERT INTO ", autofill::entities_metadata::kTableName,
              " SELECT ", autofill::entities::kGuid, ", ",
              entities_metadata::kUseCount, ", ", entities_metadata::kUseDate,
              ", ", entities_metadata::kDateModified, " FROM ",
              autofill::entities::kTableName})) &&
         DropColumn(db(), entities::kTableName, entities_metadata::kUseCount) &&
         DropColumn(db(), entities::kTableName, entities_metadata::kUseDate) &&
         DropColumn(db(), entities::kTableName,
                    entities_metadata::kDateModified) &&
         transaction.Commit();
}

bool EntityTable::AddAttribute(const EntityInstance& entity,
                               const AttributeInstance& attribute) {
  for (FieldType type :
       attribute.type().storable_field_types(/*pass_key=*/{})) {
    sql::Statement s;
    InsertBuilder(db(), s, attributes::kTableName,
                  {attributes::kEntityGuid, attributes::kAttributeType,
                   attributes::kFieldType, attributes::kValueEncrypted,
                   attributes::kVerificationStatus});
    s.BindString(0, *entity.guid());
    s.BindString(1, attribute.type().name_as_string());
    s.BindInt(2, type);
    if (std::string encrypted_value; encryptor()->EncryptString16(
            attribute.GetRawInfo(type), &encrypted_value)) {
      base::UmaHistogramBoolean("Autofill.Ai.EntityTable.EncryptStatus", true);
      s.BindString(3, encrypted_value);
    } else {
      base::UmaHistogramBoolean("Autofill.Ai.EntityTable.EncryptStatus", false);
      return false;
    }
    s.BindInt(4, static_cast<int>(attribute.GetVerificationStatus(type)));
    if (!s.Run()) {
      return false;
    }
  }
  return true;
}

bool EntityTable::AddEntityMetadata(
    const EntityInstance::EntityMetadata& metadata) {
  sql::Statement s;
  InsertBuilder(
      db(), s, entities_metadata::kTableName,
      {entities_metadata::kEntityGuid, entities_metadata::kUseCount,
       entities_metadata::kUseDate, entities_metadata::kDateModified});
  s.BindString(0, *metadata.guid);
  s.BindInt64(1, metadata.use_count);
  s.BindTime(2, metadata.use_date);
  s.BindInt64(3, metadata.date_modified.ToTimeT());
  return s.Run();
}

bool EntityTable::RemoveEntityMetadata(const EntityInstance::EntityId& guid) {
  return DeleteWhereColumnEq(db(), entities_metadata::kTableName,
                             entities_metadata::kEntityGuid, *guid);
}

bool EntityTable::AddOrUpdateEntityMetadata(
    const EntityInstance::EntityMetadata& metadata) {
  sql::Transaction transaction(db());
  return transaction.Begin() && RemoveEntityMetadata(metadata.guid) &&
         AddEntityMetadata(metadata) && transaction.Commit();
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
  InsertBuilder(db(), s, entities::kTableName,
                {entities::kGuid, entities::kEntityType, entities::kNickname,
                 entities::kRecordType, entities::kAttributesReadOnly,
                 entities::kFrecencyOverride});
  s.BindString(0, *entity.guid());
  s.BindString(1, entity.type().name_as_string());
  s.BindString(2, entity.nickname());
  s.BindInt(3, base::to_underlying(entity.record_type()));
  s.BindBool(4, entity.are_attributes_read_only().value());
  s.BindString(5, entity.frecency_override(/*pass_key=*/{}));

  if (!s.Run()) {
    return false;
  }
  // Add the entity's metadata.
  if (!AddEntityMetadata(entity.metadata())) {
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

bool EntityTable::DeleteEntityInstances(
    EntityInstance::RecordType record_type) {
  sql::Transaction transaction(db());
  return transaction.Begin() &&
         DeleteWhereColumnEq(db(), entities::kTableName, entities::kRecordType,
                             static_cast<int>(record_type)) &&
         transaction.Commit();
}

bool EntityTable::RemoveEntityInstance(const EntityInstance::EntityId& guid) {
  HandleTestSwitchesIfNeeded(db(), *this);

  sql::Transaction transaction(db());
  return transaction.Begin() &&
         DeleteWhereColumnEq(db(), attributes::kTableName,
                             attributes::kEntityGuid, *guid) &&
         DeleteWhereColumnEq(db(), entities::kTableName, entities::kGuid,
                             *guid) &&
         DeleteWhereColumnEq(db(), autofill::entities_metadata::kTableName,
                             autofill::entities_metadata::kEntityGuid, *guid) &&
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
  SelectBuilder(db(), s, entities_metadata::kTableName,
                {entities_metadata::kEntityGuid},
                "WHERE date_modified >= ? AND date_modified < ?");
  s.BindInt64(0, delete_begin.ToTimeT());
  s.BindInt64(1, delete_end.ToTimeT());
  std::vector<EntityInstance::EntityId> guids;
  while (s.Step()) {
    guids.emplace_back(s.ColumnString(0));
  }
  if (!s.Succeeded()) {
    return false;
  }

  sql::Transaction transaction(db());
  return transaction.Begin() &&
         std::ranges::all_of(guids,
                             [this](const EntityInstance::EntityId& guid) {
                               return RemoveEntityInstance(guid);
                             }) &&
         transaction.Commit();
}

bool EntityTable::EntityInstanceExists(
    const EntityInstance::EntityId& guid) const {
  sql::Statement s;
  return SelectByGuid(db(), s, entities::kTableName, {entities::kGuid},
                      *guid) &&
         s.Succeeded();
}

std::optional<EntityInstance::EntityMetadata> EntityTable::GetEntityMetadata(
    const EntityInstance::EntityId& guid) const {
  sql::Statement s;
  SelectBuilder(db(), s, entities_metadata::kTableName,
                {entities_metadata::kEntityGuid, entities_metadata::kUseCount,
                 entities_metadata::kUseDate, entities_metadata::kDateModified},
                "WHERE entity_guid = ?");
  s.BindString(0, *guid);

  if (!s.Step()) {
    return std::nullopt;
  }

  EntityInstance::EntityId entity_guid(s.ColumnString(0));
  size_t use_count = s.ColumnInt64(1);
  base::Time use_date = s.ColumnTime(2);
  base::Time date_modified = base::Time::FromTimeT(s.ColumnInt64(3));

  if (!s.Succeeded()) {
    return std::nullopt;
  }

  return EntityInstance::EntityMetadata{.guid = entity_guid,
                                        .date_modified = date_modified,
                                        .use_count = use_count,
                                        .use_date = use_date};
}

std::map<EntityInstance::EntityId, EntityInstance::EntityMetadata>
EntityTable::GetSyncedMetadata() const {
  std::map<EntityInstance::EntityId, EntityInstance::EntityMetadata>
      all_metadata = LoadMetadata();
  // Keeping only kWallet entities is not enough, because it does not handle
  // orphan metadata. Hence we are removing kLocal metadata entities from the
  // full set of metadata entries.
  for (const EntityInstance& local_entity :
       GetEntityInstances(EntityInstance::RecordType::kLocal)) {
    all_metadata.erase(local_entity.guid());
  }
  return all_metadata;
}

std::map<EntityInstance::EntityId, EntityInstance::EntityMetadata>
EntityTable::LoadMetadata() const {
  std::map<EntityInstance::EntityId, EntityInstance::EntityMetadata>
      metadata_records;
  sql::Statement s;
  SelectBuilder(db(), s, autofill::entities_metadata::kTableName,
                {autofill::entities_metadata::kEntityGuid,
                 autofill::entities_metadata::kUseCount,
                 autofill::entities_metadata::kUseDate,
                 autofill::entities_metadata::kDateModified});

  while (s.Step()) {
    EntityInstance::EntityId entity_guid(s.ColumnString(0));
    size_t use_count = s.ColumnInt64(1);
    base::Time use_date = s.ColumnTime(2);
    base::Time date_modified = base::Time::FromTimeT(s.ColumnInt64(3));
    metadata_records[entity_guid] =
        EntityInstance::EntityMetadata{.guid = entity_guid,
                                       .date_modified = date_modified,
                                       .use_count = use_count,
                                       .use_date = use_date};
  }
  if (!s.Succeeded()) {
    return {};
  }
  return metadata_records;
}

std::map<EntityInstance::EntityId,
         std::map<std::string, std::vector<EntityTable::AttributeRecord>>>
EntityTable::LoadAttributes() const {
  std::map<EntityInstance::EntityId,
           std::map<std::string, std::vector<AttributeRecord>>>
      attribute_records;
  sql::Statement s;
  SelectBuilder(db(), s, attributes::kTableName,
                {attributes::kEntityGuid, attributes::kAttributeType,
                 attributes::kFieldType, attributes::kValueEncrypted,
                 attributes::kVerificationStatus});

  // LINT.IfChange(DecryptionStatus)
  enum class DecryptionStatus {
    // Decryption was successful.
    kSuccess = 0,
    // Temporary error (e.g. The decryption system was not available).
    kTemporaryFailure = 1,
    // The decryption key was lost or the data to be decrypted was corrupt.
    kPermanentFailure = 2,
    kMaxValue = kPermanentFailure,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiDecryptStatus)

  while (s.Step()) {
    EntityInstance::EntityId entity_guid(s.ColumnString(0));
    std::string attribute_type_name = s.ColumnString(1);
    std::underlying_type_t<FieldType> underlying_field_type = s.ColumnInt(2);
    std::u16string decrypted_value;
    os_crypt_async::Encryptor::DecryptFlags flag;
    bool decryption_result = encryptor()->DecryptString16(
        s.ColumnString(3), &decrypted_value, &flag);
    base::UmaHistogramBoolean("Autofill.Ai.EntityTable.DecryptStatus",
                              decryption_result);
    DecryptionStatus decryption_status = DecryptionStatus::kSuccess;
    if (!decryption_result) {
      decryption_status = flag.temporarily_unavailable
                              ? DecryptionStatus::kTemporaryFailure
                              : DecryptionStatus::kPermanentFailure;
    }
    base::UmaHistogramEnumeration("Autofill.Ai.EntityTable.DecryptStatus2",
                                  decryption_status);
    if (!decryption_result) {
      continue;
    }
    std::underlying_type_t<VerificationStatus> underlying_verification_status =
        s.ColumnInt(4);
    attribute_records[std::move(entity_guid)][std::move(attribute_type_name)]
        .push_back({.field_type = underlying_field_type,
                    .value = std::move(decrypted_value),
                    .verification_status = underlying_verification_status});
  }
  if (!s.Succeeded()) {
    return {};
  }
  return attribute_records;
}

std::vector<EntityInstance> EntityTable::GetEntityInstances(
    std::optional<EntityInstance::RecordType> record_type) const {
  HandleTestSwitchesIfNeeded(db(), const_cast<EntityTable&>(*this));

  // Collects all attributes, keyed by the owning entity's GUID and the
  // `AttributeTypeName` of the attribute.
  std::map<EntityInstance::EntityId,
           std::map<std::string, std::vector<EntityTable::AttributeRecord>>>
      attribute_records = LoadAttributes();

  // Collects all metadata, keyed by the owning entity's GUID.
  std::map<EntityInstance::EntityId, EntityInstance::EntityMetadata>
      metadata_records = LoadMetadata();

  const std::string where =
      record_type.has_value()
          ? base::StrCat(
                {"WHERE ", entities::kRecordType, "= ",
                 base::NumberToString(base::to_underlying(*record_type))})
          : "";
  // Collects all entities and populates them with the attributes from the
  // previous query.
  std::vector<EntityInstance> entities;
  sql::Statement s;
  SelectBuilder(db(), s, entities::kTableName,
                {entities::kGuid, entities::kEntityType, entities::kNickname,
                 entities::kRecordType, entities::kAttributesReadOnly,
                 entities::kFrecencyOverride},
                where);

  while (s.Step()) {
    EntityInstance::EntityId guid(s.ColumnString(0));
    std::string type_name = s.ColumnString(1);
    std::string nickname = s.ColumnString(2);
    std::underlying_type_t<EntityInstance::RecordType> underlying_record_type =
        s.ColumnInt(3);
    EntityInstance::AreAttributesReadOnly are_attributes_read_only =
        EntityInstance::AreAttributesReadOnly(s.ColumnBool(4));
    std::string frecency_override = s.ColumnString(5);

    auto attributes = attribute_records.extract(guid);
    auto metadata = metadata_records.extract(guid);
    if (!attributes || !metadata) {
      continue;
    }
    if (std::optional<EntityInstance> e = ValidateInstance(
            type_name, std::move(guid), std::move(nickname),
            metadata.mapped().date_modified, metadata.mapped().use_count,
            metadata.mapped().use_date, underlying_record_type,
            std::move(attributes.mapped()), are_attributes_read_only,
            std::move(frecency_override))) {
      entities.push_back(*std::move(e));
    }
  }
  if (!s.Succeeded()) {
    return {};
  }
  return entities;
}

std::optional<EntityInstance> EntityTable::ValidateInstance(
    std::string_view type_name,
    EntityInstance::EntityId guid,
    std::string nickname,
    base::Time date_modified,
    int use_count,
    base::Time use_date,
    std::underlying_type_t<EntityInstance::RecordType> underlying_record_type,
    std::map<std::string, std::vector<AttributeRecord>> attribute_records,
    EntityInstance::AreAttributesReadOnly are_attributes_read_only,
    std::string frecency_override) const {
  // An attribute's field type must never be UNKNOWN_TYPE - otherwise we will
  // discard its value here.
  static_assert(
      !FieldTypeSet(DenseSet<AttributeType>::all(), &AttributeType::field_type)
           .contains(UNKNOWN_TYPE));

  std::optional<EntityType> entity_type = StringToEntityType(type_name);
  std::optional<EntityInstance::RecordType> record_type =
      ToSafeRecordType(underlying_record_type);
  if (!entity_type || guid->empty() || !record_type) {
    return std::nullopt;
  }

  std::vector<AttributeInstance> attributes;

  for (const auto& [attribute_type_name, records] : attribute_records) {
    if (std::optional<AttributeType> attribute_type =
            StringToAttributeType(*entity_type, attribute_type_name)) {
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
                        std::move(nickname), date_modified, use_count, use_date,
                        *record_type, are_attributes_read_only,
                        frecency_override);
}

}  // namespace autofill
