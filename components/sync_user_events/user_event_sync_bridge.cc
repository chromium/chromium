// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_user_events/user_event_sync_bridge.h"

#include <set>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

using sync_pb::ModelTypeState;
using sync_pb::UserEventSpecifics;
using IdList = ModelTypeStore::IdList;
using Record = ModelTypeStore::Record;
using RecordList = ModelTypeStore::RecordList;
using WriteBatch = ModelTypeStore::WriteBatch;

namespace {

std::string GetStorageKeyFromSpecifics(const UserEventSpecifics& specifics) {
  // Force Big Endian, this means newly created keys are last in sort order,
  // which allows leveldb to append new writes, which it is best at.
  // TODO(skym): Until we force |event_time_usec| to never conflict, this has
  // the potential for errors.
  std::string key(8, 0);
  base::WriteBigEndian(&key[0], specifics.event_time_usec());
  return key;
}

int64_t GetEventTimeFromStorageKey(const std::string& storage_key) {
  int64_t event_time;
  base::ReadBigEndian(&storage_key[0], &event_time);
  return event_time;
}

std::unique_ptr<EntityData> MoveToEntityData(
    std::unique_ptr<UserEventSpecifics> specifics) {
  auto entity_data = std::make_unique<EntityData>();
  entity_data->name = base::NumberToString(specifics->event_time_usec());
  entity_data->specifics.set_allocated_user_event(specifics.release());
  return entity_data;
}

}  // namespace

UserEventSyncBridge::UserEventSyncBridge(
    OnceModelTypeStoreFactory store_factory,
    std::unique_ptr<ModelTypeChangeProcessor> change_processor,
    GlobalIdMapper* global_id_mapper)
    : ModelTypeSyncBridge(std::move(change_processor)),
      global_id_mapper_(global_id_mapper) {
  DCHECK(global_id_mapper_);
  std::move(store_factory)
      .Run(USER_EVENTS, base::BindOnce(&UserEventSyncBridge::OnStoreCreated,
                                       weak_ptr_factory_.GetWeakPtr()));
  global_id_mapper_->AddGlobalIdChangeObserver(
      base::BindRepeating(&UserEventSyncBridge::HandleGlobalIdChange,
                          weak_ptr_factory_.GetWeakPtr()));
}

UserEventSyncBridge::~UserEventSyncBridge() = default;

std::unique_ptr<MetadataChangeList>
UserEventSyncBridge::CreateMetadataChangeList() {
  return WriteBatch::CreateMetadataChangeList();
}

base::Optional<ModelError> UserEventSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  DCHECK(entity_data.empty());
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(!change_processor()->TrackedAccountId().empty());
  return ApplySyncChanges(std::move(metadata_change_list),
                          std::move(entity_data));
}

base::Optional<ModelError> UserEventSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  std::set<int64_t> deleted_event_times;
  for (const std::unique_ptr<EntityChange>& change : entity_changes) {
    DCHECK_EQ(EntityChange::ACTION_DELETE, change->type());
    batch->DeleteData(change->storage_key());
    deleted_event_times.insert(
        GetEventTimeFromStorageKey(change->storage_key()));
  }

  // Because we receive ApplySyncChanges with deletions when our commits are
  // confirmed, this is the perfect time to cleanup our in flight objects which
  // are no longer in flight.
  base::EraseIf(in_flight_nav_linked_events_,
                [&deleted_event_times](
                    const std::pair<int64_t, sync_pb::UserEventSpecifics> kv) {
                  return base::Contains(deleted_event_times,
                                        kv.second.event_time_usec());
                });

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&UserEventSyncBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
  return {};
}

void UserEventSyncBridge::GetData(StorageKeyList storage_keys,
                                  DataCallback callback) {
  store_->ReadData(
      storage_keys,
      base::BindOnce(&UserEventSyncBridge::OnReadData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UserEventSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  store_->ReadAllData(base::BindOnce(&UserEventSyncBridge::OnReadAllData,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

std::string UserEventSyncBridge::GetClientTag(const EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string UserEventSyncBridge::GetStorageKey(const EntityData& entity_data) {
  return GetStorageKeyFromSpecifics(entity_data.specifics.user_event());
}

void UserEventSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  if (delete_metadata_change_list) {
    store_->DeleteAllDataAndMetadata(base::BindOnce(
        &UserEventSyncBridge::OnCommit, weak_ptr_factory_.GetWeakPtr()));
  }
}

void UserEventSyncBridge::RecordUserEvent(
    std::unique_ptr<UserEventSpecifics> specifics) {
  DCHECK(!specifics->has_user_consent());
  if (store_) {
    RecordUserEventImpl(std::move(specifics));
    return;
  }
}

// static
std::string UserEventSyncBridge::GetStorageKeyFromSpecificsForTest(
    const UserEventSpecifics& specifics) {
  return GetStorageKeyFromSpecifics(specifics);
}

std::unique_ptr<ModelTypeStore> UserEventSyncBridge::StealStoreForTest() {
  return std::move(store_);
}

void UserEventSyncBridge::RecordUserEventImpl(
    std::unique_ptr<UserEventSpecifics> specifics) {
  DCHECK(store_);

  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  std::string storage_key = GetStorageKeyFromSpecifics(*specifics);
  // There are two scenarios we need to guard against here. First, the given
  // user even may have been read from an old global_id timestamp off of a
  // navigation, which has already been re-written. In this case, we should be
  // able to look up the latest/best global_id to use right now, and update as
  // such. The other scenario is that the navigation is going to be updated in
  // the future, and the current global_id, while valid for now, is never going
  // to make it to the server, and will need to be fixed. To handle this
  // scenario, we store a specifics copy in |in in_flight_nav_linked_events_|,
  // and will re-record in HandleGlobalIdChange.
  if (specifics->has_navigation_id()) {
    int64_t latest_global_id =
        global_id_mapper_->GetLatestGlobalId(specifics->navigation_id());
    specifics->set_navigation_id(latest_global_id);
    in_flight_nav_linked_events_.insert(
        std::make_pair(latest_global_id, *specifics));
  }

  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  batch->WriteData(storage_key, specifics->SerializeAsString());

  DCHECK(change_processor()->IsTrackingMetadata());
  change_processor()->Put(storage_key, MoveToEntityData(std::move(specifics)),
                          batch->GetMetadataChangeList());

  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&UserEventSyncBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void UserEventSyncBridge::OnStoreCreated(
    const base::Optional<ModelError>& error,
    std::unique_ptr<ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllMetadata(base::BindOnce(
      &UserEventSyncBridge::OnReadAllMetadata, weak_ptr_factory_.GetWeakPtr()));
}

void UserEventSyncBridge::OnReadAllMetadata(
    const base::Optional<ModelError>& error,
    std::unique_ptr<MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
  } else {
    change_processor()->ModelReadyToSync(std::move(metadata_batch));
  }
}

void UserEventSyncBridge::OnCommit(const base::Optional<ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void UserEventSyncBridge::OnReadData(DataCallback callback,
                                     const base::Optional<ModelError>& error,
                                     std::unique_ptr<RecordList> data_records,
                                     std::unique_ptr<IdList> missing_id_list) {
  OnReadAllData(std::move(callback), error, std::move(data_records));
}

void UserEventSyncBridge::OnReadAllData(
    DataCallback callback,
    const base::Optional<ModelError>& error,
    std::unique_ptr<RecordList> data_records) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  auto batch = std::make_unique<MutableDataBatch>();
  for (const Record& r : *data_records) {
    auto specifics = std::make_unique<UserEventSpecifics>();

    if (specifics->ParseFromString(r.value)) {
      DCHECK_EQ(r.id, GetStorageKeyFromSpecifics(*specifics));
      batch->Put(r.id, MoveToEntityData(std::move(specifics)));
    } else {
      change_processor()->ReportError(
          {FROM_HERE, "Failed deserializing user events."});
      return;
    }
  }
  std::move(callback).Run(std::move(batch));
}

void UserEventSyncBridge::HandleGlobalIdChange(int64_t old_global_id,
                                               int64_t new_global_id) {
  DCHECK_NE(old_global_id, new_global_id);

  // Store specifics in a temp vector while erasing, as |RecordUserEvent()| will
  // insert new values into |in_flight_nav_linked_events_|. While insert should
  // not invalidate a std::multimap's iterator, and the updated global_id should
  // not be within our given range, this approach seems less error prone.
  std::vector<std::unique_ptr<UserEventSpecifics>> affected;

  auto range = in_flight_nav_linked_events_.equal_range(old_global_id);
  for (auto iter = range.first; iter != range.second;) {
    DCHECK_EQ(old_global_id, iter->second.navigation_id());
    affected.emplace_back(std::make_unique<UserEventSpecifics>(iter->second));
    iter = in_flight_nav_linked_events_.erase(iter);
  }

  for (std::unique_ptr<UserEventSpecifics>& specifics : affected) {
    specifics->set_navigation_id(new_global_id);
    RecordUserEvent(std::move(specifics));
  }
}

}  // namespace syncer
