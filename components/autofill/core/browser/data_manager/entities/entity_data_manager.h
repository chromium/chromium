// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ENTITIES_ENTITY_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ENTITIES_ENTITY_DATA_MANAGER_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
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
class EntityDataManager : public KeyedService, public WebDataServiceConsumer {
 public:
  using LoadCallback = base::OnceCallback<void(std::vector<EntityInstance>)>;

  explicit EntityDataManager(
      scoped_refptr<AutofillWebDataService> profile_database);
  EntityDataManager(const EntityDataManager&) = delete;
  EntityDataManager& operator=(const EntityDataManager&) = delete;
  ~EntityDataManager() override;

  // Adds a new entity, updates an existing entity, or removes an entity.
  // Entities are identified by their UUID for update and removal purposes.
  virtual void AddEntityInstance(const EntityInstance& entity);
  virtual void UpdateEntityInstance(const EntityInstance& entity);
  virtual void RemoveEntityInstance(const base::Uuid& guid);

  // Retrieves the valid entity instances from the database and calls `cb`
  // asynchronously with the result.
  //
  // See `EntityTable::GetEntityInstances()` for details on what "valid" means.
  //
  // It is guaranteed that `cb` is called eventually; if the query is
  // unsuccessful, `cb` is called with an empty vector.
  virtual void LoadEntityInstances(LoadCallback cb);

 protected:
  void RegisterPendingQuery(WebDataServiceBase::Handle handle, LoadCallback cb);

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

 private:
  // Non-null except perhaps in TestEntityDataManager, which overrides all
  // functions that access .
  const scoped_refptr<AutofillWebDataService> webdata_service_;

  std::map<WebDataServiceBase::Handle, LoadCallback> pending_queries_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ENTITIES_ENTITY_DATA_MANAGER_H_
