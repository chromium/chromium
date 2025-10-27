// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_TABLE_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace autofill {

class AttributeInstance;

// This class manages the tables to store `EntityInstance` objects and their
// `AttributeInstance`s within the SQLite database passed to the constructor. It
// expects the following schemas:
//
// -----------------------------------------------------------------------------
// entities             Contains entity instances.
//
//   guid                   Uniquely identifies the entity instance (primary
//                          key).
//   entity_type            The instance's entity type, represented as string
//                          EntityType::name_as_string().
//   nickname               The instance's string nickname.
//   record_type            Information about the original storage of the
//                          entity (local/server).
//   attributes_read_only   Boolean flag backed by an integer. If 1,
//                          the attributes of the entity instance are not
//                          editable by the user.
// -----------------------------------------------------------------------------
// entities_metadata    Contains metadata for entity instances.
//
//   entity_guid            Uniquely identifies the entity instance (primary
//                          key as well as foreign key into the entities table).
//   use_count              The number of times that this instance has been
//                          used.
//   use_date               The last time that this instance was used to fill a
//                          form.
//   date_modified          The date on which this instance was last modified,
//                          in time_t.
// -----------------------------------------------------------------------------
// attributes               Contains the attribute instances of the entity
//                          instances from the `entities` table.
//
//  entity_guid             Identifies the owning entity instance (primary
//                          key as well as foreign key into the entities table).
//  attribute_type          The instance's attribute type, represented as string
//                          AttributeType::name_as_string().
//  field_type              The FieldType, represented by its integer value in
//                          the FieldType enum.
//  value_encrypted         The encrypted string value of the attribute
//                          identified by (entity_guid, attribute_type,
//                          field_type).
//  verification_status     Each value stored in an attribute has an additional
//                          verification status that indicates if Autofill
//                          parsed the value out of an unstructured value, or if
//                          Autofill formatted the value from a structured
//                          subcomponent, or if the value was observed in a form
//                          submission, or even validated by the user in the
//                          settings.
// -----------------------------------------------------------------------------
class EntityTable : public WebDatabaseTable {
 public:
  EntityTable();
  EntityTable(const EntityTable&) = delete;
  EntityTable& operator=(const EntityTable&) = delete;
  ~EntityTable() override;

  static EntityTable* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Returns true if removing the entity and then re-adding it is successful.
  bool AddOrUpdateEntityInstance(const EntityInstance& entity);

  // Deletes all entities with record type `record_type`. Returns true on
  // success.
  bool DeleteEntityInstances(EntityInstance::RecordType record_type);

  // Returns true if removing the entity succeeded, even if there were zero or
  // multiple matches.
  bool RemoveEntityInstance(const EntityInstance::EntityId& guid);

  // Removes all stored entities and their attributes that were modified in the
  // given range [`delete_begin`, `delete_end`).
  //
  // Prefer this function over iterating over GetEntityInstances() and calling
  // RemoveEntityInstance() because this function also removes invalid entities.
  bool RemoveEntityInstancesModifiedBetween(base::Time delete_begin,
                                            base::Time delete_end);

  // Returns true if an entity instance with the given `guid` exists in the
  // database.
  bool EntityInstanceExists(const EntityInstance::EntityId& guid) const;

  // Adds or updates the entity `metadata` in the `entities_metadata` table.
  // Returns true on success.
  bool AddOrUpdateEntityMetadata(
      const EntityInstance::EntityMetadata& metadata);

  // Returns true if removing the entity metadata succeeded.
  bool RemoveEntityMetadata(const EntityInstance::EntityId& guid);

  // Returns the entity metadata for the given `guid`.
  std::optional<EntityInstance::EntityMetadata> GetEntityMetadata(
      const EntityInstance::EntityId& guid) const;

  // Returns the valid entity instances; ignores invalid instances.
  //
  // An instance is valid only if all the following is true:
  // - EntityTypeNames and AttributeTypeNames are known in the schema
  //   (based on their integer representation, not their string value).
  // - GUIDs are in a valid format.
  // - At least one of the necessary-attributes constraints from the schema is
  //   satisfied.
  // Results can be filtered by `record_type`, if provided.
  std::vector<EntityInstance> GetEntityInstances(
      std::optional<EntityInstance::RecordType> record_type =
          std::nullopt) const;

  // Returns the content of `autofill_ai_entities_metadata` table that is synced
  // by the server i.e. all metadata entries without a corresponding `local`
  // entity.
  std::map<EntityInstance::EntityId, EntityInstance::EntityMetadata>
  GetSyncedMetadata() const;

 private:
  // Contains information about an attribute stored in the `attributes` table.
  // Note that this doesn't contain the owning entity's GUID or the attribute
  // type because when we load this table and use this structure those two
  // pieces of information are the key to access objects of the following class.
  struct AttributeRecord {
    std::underlying_type_t<FieldType> field_type;
    std::u16string value;
    std::underlying_type_t<VerificationStatus> verification_status;
  };

  // In this version `use_count`, `use_date`, and `date_modified` were
  // moved from `autofill_ai_entities` to `autofill_ai_entities_metadata`.
  bool MigrateToVersion147AddEntitiesMetadataTable();

  // Returns true if adding the entity succeeded.
  // It does not validate the entity itself, but it does check that no such
  // entity with the same GUID exists.
  bool AddEntityInstance(const EntityInstance& entity);

  // Returns true if adding the entity `metadata` to the `entities_metadata`
  // table succeeded.
  bool AddEntityMetadata(const EntityInstance::EntityMetadata& metadata);

  // Returns true if adding the attribute to the `attributes` table succeeded.
  bool AddAttribute(const EntityInstance& entity,
                    const AttributeInstance& attribute);

  // Loads the content of `attributes` table into memory. The 2D map returned is
  // keyed by UUID and AttributeTypeName of the loaded attributes.
  std::map<EntityInstance::EntityId,
           std::map<std::string, std::vector<AttributeRecord>>>
  LoadAttributes() const;

  // Loads the content of `autofill_ai_entities_metadata` table into memory. The
  // map returned is keyed by EntityId of the loaded attributes.
  std::map<EntityInstance::EntityId, EntityInstance::EntityMetadata>
  LoadMetadata() const;

  // Attempts to create an `EntityInstance` object provided information loaded
  // from the database. Returns the instance itself if creation was successful
  // and `std::nullopt` otherwise.
  std::optional<EntityInstance> ValidateInstance(
      std::string_view type_name,
      EntityInstance::EntityId guid,
      std::string nickname,
      base::Time date_modified,
      int use_count,
      base::Time use_date,
      std::underlying_type_t<EntityInstance::RecordType>
          underlying_storage_type,
      std::map<std::string, std::vector<AttributeRecord>> attribute_records,
      EntityInstance::AreAttributesReadOnly are_attributes_read_only,
      std::string frecency_override) const;

  friend class EntityTableTestApi;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_TABLE_H_
