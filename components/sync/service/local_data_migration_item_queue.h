// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_LOCAL_DATA_MIGRATION_ITEM_QUEUE_H_
#define COMPONENTS_SYNC_SERVICE_LOCAL_DATA_MIGRATION_ITEM_QUEUE_H_

#include <map>
#include <tuple>

#include "base/scoped_observation.h"
#include "base/time/default_clock.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_service_observer.h"

namespace syncer {

class SyncService;
class DataTypeManager;

// This class is used to migrate data to account storage once the sync service
// activates, if the user is eligible for account storage.
class LocalDataMigrationItemQueue : public SyncServiceObserver {
 public:
  // `sync_service` and `data_type_manager` must not be null and must outlive
  // this class.
  LocalDataMigrationItemQueue(SyncService* sync_service,
                              DataTypeManager* data_type_manager);
  ~LocalDataMigrationItemQueue() override;

  // Adds the `items` to the queue and triggers `OnStateChanged()` once. If sync
  // is disabled or the user has consented to sync, the items are removed again.
  void TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(
      DataType data_type,
      std::vector<LocalDataItemModel::DataId> items);

  // SyncServiceObserver:
  void OnStateChanged(SyncService* sync_service) override;
  void OnSyncShutdown(SyncService* sync_service) override;

  void SetClockForTesting(base::Clock* clock);

 private:
  const raw_ptr<SyncService> sync_service_;
  const raw_ptr<DataTypeManager> data_type_manager_;
  raw_ptr<base::Clock> clock_;
  // TODO (crbug.com/385310859): Create a function that converts from `DataId`
  // to `DataType` so that we don't need to store this information.
  std::map<syncer::LocalDataItemModel::DataId, std::tuple<DataType, base::Time>>
      items_;
  base::ScopedObservation<SyncService, SyncServiceObserver>
      sync_service_observer_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_LOCAL_DATA_MIGRATION_ITEM_QUEUE_H_
