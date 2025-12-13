// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_INSTANCE_CLEANER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_INSTANCE_CLEANER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/sync/service/sync_service_observer.h"

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace autofill {

class EntityDataManager;

// EntityInstanceCleaner is responsible for applying cleanups on browser
// startup, after sync is ready (if applicable). Note that only local entities
// are cleaned up.
class EntityInstanceCleaner : public syncer::SyncServiceObserver {
 public:
  EntityInstanceCleaner(EntityDataManager* entity_data_manager,
                        syncer::SyncService* sync_service,
                        PrefService* pref_service);
  ~EntityInstanceCleaner() override;
  EntityInstanceCleaner(const EntityInstanceCleaner&) = delete;
  EntityInstanceCleaner& operator=(const EntityInstanceCleaner&) = delete;

 private:
  friend class EntityInstanceCleanerTestApi;

  // Determines whether the cleanups should run depending on the sync state and
  // runs them if applicable. Ensures that the cleanups are run at most once
  // over multiple invocations of the functions. It also runs only once per
  // milestone.
  void MaybeCleanupLocalEntityInstancesData();

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync_service) override;
  void OnSyncShutdown(syncer::SyncService* sync_service) override;

  // Used to ensure that cleanups are only performed once per profile startup.
  bool are_cleanups_pending_ = true;
  const raw_ref<EntityDataManager> entity_data_manager_;
  const raw_ptr<syncer::SyncService> sync_service_;
  // Used to check whether deduplication was already run this milestone.
  const raw_ref<PrefService> pref_service_;

  // Observer Sync, so cleanups are not run before any new data was synced down
  // on startup.
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_INSTANCE_CLEANER_H_
