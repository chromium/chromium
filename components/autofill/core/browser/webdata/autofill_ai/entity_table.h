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
#include "components/autofill/core/browser/field_types.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace base {
class Uuid;
}

namespace autofill {

class AttributeInstance;
class EntityInstance;

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
//   date_modified          The date on which this instance was last modified,
//                          in time_t.
// -----------------------------------------------------------------------------
// attributes               Contains the attribute instances of the entity
//                          instances from the `entities` table.
//
//  entity_guid             Identifies the owning entity instance (it is a
//                          foreign key).
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

  // Returns true if removing the entity succeeded, even if there were zero or
  // multiple matches.
  bool RemoveEntityInstance(const base::Uuid& guid);

  // Removes all stored entities and their attributes that were modified in the
  // given range [`delete_begin`, `delete_end`).
  //
  // Prefer this function over iterating over GetEntityInstances() and calling
  // RemoveEntityInstance() because this function also removes invalid entities.
  bool RemoveEntityInstancesModifiedBetween(base::Time delete_begin,
                                            base::Time delete_end);

  // Returns the valid entity instances; ignores invalid instances.
  //
  // An instance is valid only if all the following is true:
  // - EntityTypeNames and AttributeTypeNames are known in the schema
  //   (based on their integer representation, not their string value).
  // - GUIDs are in a valid format.
  // - At least one of the necessary-attributes constraints from the schema is
  //   satisfied.
  std::vector<EntityInstance> GetEntityInstances() const;

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

  // Returns true if adding the entity succeeded.
  // It does not validate the entity itself, but it does check that no such
  // entity with the same GUID exists.
  bool AddEntityInstance(const EntityInstance& entity);

  // Returns true if adding the attribute to the `attributes` table succeeded.
  bool AddAttribute(const EntityInstance& entity,
                    const AttributeInstance& attribute);

  // Loads the content of `attributes` table into memory. The 2D map returned is
  // keyed by UUID and AttributeTypeName of the loaded attributes.
  std::map<base::Uuid, std::map<std::string, std::vector<AttributeRecord>>>
  LoadAttributes() const;

  // Attempts to create an `EntityInstance` object provided information loaded
  // from the database. Returns the instance itself if creation was successful
  // and `std::nullopt` otherwise.
  std::optional<EntityInstance> ValidateInstance(
      std::string_view type_name,
      base::Uuid guid,
      std::string nickname,
      base::Time date_modified,
      int use_count,
      base::Time use_date,
      std::map<std::string, std::vector<AttributeRecord>> attribute_records)
      const;

  friend class EntityTableTestApi;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_TABLE_H_
