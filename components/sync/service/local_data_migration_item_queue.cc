// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/local_data_migration_item_queue.h"

#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/data_type_manager.h"
#include "components/sync/service/sync_service.h"

namespace syncer {

namespace {

// The time limit for activating the data type is set to 50 minutes, as this is
// in line with the `SigninPromoTabHelper` waiting for a sign in event.
constexpr base::TimeDelta kTimeLimitForActivatingDataType = base::Minutes(50);

// Removes items which had been added to the queue before the time limit
// has been exceeded, or have a data type that is non-preferred.
void RemoveExpiredItemsAndItemsOfNonPreferredDataTypes(
    std::map<LocalDataItemModel::DataId, std::tuple<DataType, base::Time>>&
        items,
    const DataTypeSet& preferred_data_types,
    const base::Time& current_time_stamp) {
  // Use this to keep the items for which the data type is not active yet.
  std::map<LocalDataItemModel::DataId, std::tuple<DataType, base::Time>>
      items_to_keep;

  for (auto& item : items) {
    DataType data_type = std::get<0>(item.second);
    base::Time initialized_time_stamp = std::get<1>(item.second);
    LocalDataItemModel::DataId data_id = item.first;

    // Do not include the item if the time limit since the initialization
    // of its move has been exceeded. This can happen for example if a user
    // with custom passphrase enabled signs in after a sign in promo which
    // initializes this queue, but the user still has to enter their
    // passphrase. As they may forget that entering their passphrase would
    // move the data, we do nothing instead. Also, remove items which have a
    // data type which is non-preferred.
    if (current_time_stamp - initialized_time_stamp <
            kTimeLimitForActivatingDataType &&
        preferred_data_types.Has(data_type)) {
      items_to_keep[data_id] = std::move(item.second);
    }
  }

  items = std::move(items_to_keep);
}

// Returns those items which have a data type that is active, so that their
// migration to account storage can be triggered. The items for which the data
// type is not active are kept in `items` in case it will be activated later,
// e.g. if the user enters their custom passphrase.
std::map<DataType, std::vector<LocalDataItemModel::DataId>>
MoveItemsOfActiveDataTypesToVector(
    std::map<LocalDataItemModel::DataId, std::tuple<DataType, base::Time>>&
        items,
    const DataTypeSet& active_data_types) {
  // Use this to keep the items for which the data type is not active yet.
  std::map<LocalDataItemModel::DataId, std::tuple<DataType, base::Time>>
      items_to_keep;
  // Items that are added here will be migrated to account storage.
  std::map<DataType, std::vector<LocalDataItemModel::DataId>> result;

  for (auto& item : items) {
    DataType data_type = std::get<0>(item.second);
    LocalDataItemModel::DataId data_id = item.first;

    // Only include those items in the vector for which the data type is already
    // active.
    if (active_data_types.Has(data_type)) {
      result[data_type].push_back(data_id);
    } else {
      items_to_keep[data_id] = std::move(item.second);
    }
  }

  items = std::move(items_to_keep);
  return result;
}

}  // namespace

LocalDataMigrationItemQueue::LocalDataMigrationItemQueue(
    SyncService* sync_service,
    DataTypeManager* data_type_manager)
    : sync_service_(sync_service),
      data_type_manager_(data_type_manager),
      clock_(base::DefaultClock::GetInstance()) {
  CHECK(sync_service);
  CHECK(data_type_manager);

  sync_service_observer_.Observe(sync_service_);
}

LocalDataMigrationItemQueue::~LocalDataMigrationItemQueue() = default;

void LocalDataMigrationItemQueue::
    TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(
        DataType data_type,
        std::vector<LocalDataItemModel::DataId> items) {
  base::Time current_timestamp = clock_->Now();
  for (const auto& item : items) {
    items_[item] = std::tuple(data_type, current_timestamp);
  }

  // Fire once in case the sync service is already active.
  OnStateChanged(sync_service_);
}

void LocalDataMigrationItemQueue::OnStateChanged(SyncService* sync_service) {
  CHECK_EQ(sync_service, sync_service_);

  // Do not move the data if the user has consented to sync in the meantime. In
  // this case, simply leave the data in local storage.
  if (sync_service->HasSyncConsent()) {
    items_.clear();
    return;
  }

  switch (sync_service->GetTransportState()) {
    case SyncService::TransportState::DISABLED:
    case SyncService::TransportState::PAUSED: {
      items_.clear();
      return;
    }
    case SyncService::TransportState::START_DEFERRED:
    case SyncService::TransportState::INITIALIZING:
    case SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
    case SyncService::TransportState::CONFIGURING:
      // Keep waiting if the sync service is not active yet.
      return;
    case SyncService::TransportState::ACTIVE:
      break;
  }

  CHECK_EQ(DataTypeManager::CONFIGURED, data_type_manager_->state());
  RemoveExpiredItemsAndItemsOfNonPreferredDataTypes(
      items_, sync_service->GetPreferredDataTypes(), clock_->Now());

  std::map<DataType, std::vector<LocalDataItemModel::DataId>> items_to_migrate =
      MoveItemsOfActiveDataTypesToVector(items_,
                                         sync_service->GetActiveDataTypes());

  if (items_to_migrate.empty()) {
    return;
  }

  // Use the data type manager directly rather than
  // `SyncService::TriggerLocalDataMigration()`, since this object is used for a
  // single data item move operation from the sign in promo. This is different
  // from batch upload, so we shouldn't record the batch upload histogram in
  // that case.
  data_type_manager_->TriggerLocalDataMigrationForItems(items_to_migrate);
}

void LocalDataMigrationItemQueue::OnSyncShutdown(SyncService* sync_service) {
  items_.clear();
}

void LocalDataMigrationItemQueue::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

size_t LocalDataMigrationItemQueue::GetItemsCountForTesting() const {
  return items_.size();
}

}  // namespace syncer
