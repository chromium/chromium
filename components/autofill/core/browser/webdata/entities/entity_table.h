// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ENTITIES_ENTITY_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ENTITIES_ENTITY_TABLE_H_

#include <vector>

#include "base/time/time.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace base {
class Uuid;
}

namespace autofill {

class EntityInstance;

// This class manages the tables to store `EntityInstance` objects and their
// `AttributeInstance`s within the SQLite database passed to the constructor. It
// expects the following schemas:
//
// -----------------------------------------------------------------------------
// entities             Contains entity instances.
//
//   guid               Uniquely identifies the entity instance (primary key).
//   type               The instance's entity type, represented as string
//                      EntityType::name_as_string().
//   nickname           The instance's string nickname.
//   date_modified      The date on which this instance was last modified, in
//                      time_t.
// -----------------------------------------------------------------------------
// attributes           Contains the attribute instances of the entity instances
//                      from the `entities` table.
//
//   entity_guid        Identifies owning entity instances (it's a foreign key).
//   type               The instance's attribute type, represented as string
//                      AttributeType::name_as_string().
//   value_encrypted    The encrypted string value of the attribute.
//   context            The format string of the attribute.
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
  // Returns true if adding the entity succeeded.
  // It does not validate the entity itself, but it does check that no such
  // entity with the same GUID exists.
  bool AddEntityInstance(const EntityInstance& entity);

  friend class EntityTableTestApi;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ENTITIES_ENTITY_TABLE_H_
