// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ENTITIES_ENTITY_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ENTITIES_ENTITY_DATA_MANAGER_H_

#include <map>
#include <memory>

#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {

// Loads, adds, updates, and removes EntityInstances.
//
// These operations are asynchronous; this is similar to
// AutocompleteHistoryManager and unlike AddressDataManager.
//
// There is at most one instance per profile. While incognito profiles have
// their own EntityDataManager instance, they use the same underlying database.
// Therefore, it is the responsibility of the callers to ensure that no data
// from an incognito session is persisted unintentionally.
class EntityDataManager : public KeyedService {
 public:
  explicit EntityDataManager(
      scoped_refptr<AutofillWebDataService> profile_database);
  EntityDataManager(const EntityDataManager&) = delete;
  EntityDataManager& operator=(const EntityDataManager&) = delete;
  ~EntityDataManager() override;

  // Adds an entity if it doesn't exist in the database yet; otherwise updates
  // it.
  void AddOrUpdateEntityInstance(EntityInstance entity);

  // Removes an entity if it exists in the database; otherwise it's a no-op.
  void RemoveEntityInstance(base::Uuid guid);

  // Removes all entities in the database whose EntityInstance::date_modified()
  // is in the range.
  // Prefer this function over iterating over GetEntityInstances() and calling
  // RemoveEntityInstance() because this function also removes invalid entities.
  void RemoveEntityInstancesModifiedBetween(base::Time delete_begin,
                                            base::Time delete_end);

  // Returns the cached valid entity instances from the database.
  //
  // The cache is populated asynchronously after the construction of this
  // EntityDataManager. Returns an empty vector until the population is
  // finished.
  //
  // See `EntityTable::GetEntityInstances()` for details on what "valid" means.
  const std::vector<EntityInstance>& GetEntityInstances() const;

 private:
  void LoadEntities();

  // Non-null except perhaps in TestEntityDataManager, which overrides all
  // functions that access it.
  const scoped_refptr<AutofillWebDataService> webdata_service_;

  // The ongoing LoadEntities() query.
  WebDataServiceBase::Handle pending_query_{};

  // The result of the last successful LoadEntities() query.
  // All entries are identifiable by their EntityInstance::guid().
  std::vector<EntityInstance> entities_;

  base::WeakPtrFactory<EntityDataManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ENTITIES_ENTITY_DATA_MANAGER_H_
