// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_user_events/user_event_sync_bridge.h"

#include <array>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/model/data_type_store_with_in_memory_cache.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"

namespace syncer {

using sync_pb::UserEventSpecifics;

namespace {

std::string GetStorageKeyFromSpecifics(const UserEventSpecifics& specifics) {
  // Force Big Endian, this means newly created keys are last in sort order,
  // which allows leveldb to append new writes, which it is best at.
  // TODO(skym): Until we force |event_time_usec| to never conflict, this has
  // the potential for errors.
  std::array<uint8_t, 8> key =
      base::U64ToBigEndian(specifics.event_time_usec());
  return std::string(key.begin(), key.end());
}

int64_t GetEventTimeFromStorageKey(const std::string& storage_key) {
  return base::U64FromBigEndian(base::as_byte_span(storage_key).first<8u>());
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
    OnceDataTypeStoreFactory store_factory,
    std::unique_ptr<DataTypeLocalChangeProcessor> change_processor,
    GlobalIdMapper* global_id_mapper)
    : DataTypeSyncBridge(std::move(change_processor)),
      global_id_mapper_(global_id_mapper) {
  DCHECK(global_id_mapper_);
  StoreWithCache::CreateAndLoad(
      std::move(store_factory), USER_EVENTS,
      base::BindOnce(&UserEventSyncBridge::OnStoreLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
  global_id_mapper_->AddGlobalIdChangeObserver(
      base::BindRepeating(&UserEventSyncBridge::HandleGlobalIdChange,
                          weak_ptr_factory_.GetWeakPtr()));
}

UserEventSyncBridge::~UserEventSyncBridge() {
  // TODO(crbug.com/362428820): Remove logging once investigation is complete.
  if (store_) {
    VLOG(1) << "UserEvents during destruction: "
            << store_->in_memory_data().size();
  }
}

std::unique_ptr<MetadataChangeList>
UserEventSyncBridge::CreateMetadataChangeList() {
  return DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<ModelError> UserEventSyncBridge::MergeFullSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  DCHECK(entity_data.empty());
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(!change_processor()->TrackedAccountId().empty());
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<ModelError> UserEventSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  CHECK(store_);

  std::unique_ptr<StoreWithCache::WriteBatch> batch =
      store_->CreateWriteBatch();
  std::set<int64_t> deleted_event_times;
  for (const std::unique_ptr<EntityChange>& change : entity_changes) {
    DCHECK_EQ(EntityChange::ACTION_DELETE, change->type());
    batch->DeleteData(change->storage_key());
    deleted_event_times.insert(
        GetEventTimeFromStorageKey(change->storage_key()));
  }

  // Because we receive ApplyIncrementalSyncChanges with deletions when our
  // commits are confirmed, this is the perfect time to cleanup our in flight
  // objects which are no longer in flight.
  std::erase_if(
      in_flight_nav_linked_events_,
      [&deleted_event_times](
          const std::pair<int64_t, sync_pb::UserEventSpecifics>& kv) {
        return deleted_event_times.contains(kv.second.event_time_usec());
      });

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&UserEventSyncBridge::OnStoreCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
  return {};
}

std::unique_ptr<DataBatch> UserEventSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  CHECK(store_);

  auto batch = std::make_unique<MutableDataBatch>();
  const std::map<std::string, UserEventSpecifics>& in_memory_data =
      store_->in_memory_data();
  for (const std::string& storage_key : storage_keys) {
    auto it = in_memory_data.find(storage_key);
    if (it != in_memory_data.end()) {
      auto specifics = std::make_unique<UserEventSpecifics>(it->second);
      batch->Put(it->first, MoveToEntityData(std::move(specifics)));
    }
  }
  return batch;
}

std::unique_ptr<DataBatch> UserEventSyncBridge::GetAllDataForDebugging() {
  CHECK(store_);

  auto batch = std::make_unique<MutableDataBatch>();
  for (const auto& [storage_key, specifics] : store_->in_memory_data()) {
    auto specifics_copy = std::make_unique<UserEventSpecifics>(specifics);
    batch->Put(storage_key, MoveToEntityData(std::move(specifics_copy)));
  }
  return batch;
}

std::string UserEventSyncBridge::GetClientTag(const EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string UserEventSyncBridge::GetStorageKey(const EntityData& entity_data) {
  return GetStorageKeyFromSpecifics(entity_data.specifics.user_event());
}

void UserEventSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  CHECK(store_);

  store_->DeleteAllDataAndMetadata(base::BindOnce(
      &UserEventSyncBridge::OnStoreCommit, weak_ptr_factory_.GetWeakPtr()));
}

void UserEventSyncBridge::RecordUserEvent(
    std::unique_ptr<UserEventSpecifics> specifics) {
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

std::unique_ptr<DataTypeStore> UserEventSyncBridge::StealStoreForTest() {
  return StoreWithCache::ExtractUnderlyingStoreForTest(std::move(store_));
}

void UserEventSyncBridge::RecordUserEventImpl(
    std::unique_ptr<UserEventSpecifics> specifics) {
  CHECK(store_);

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

  std::unique_ptr<StoreWithCache::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(storage_key, *specifics);

  DCHECK(change_processor()->IsTrackingMetadata());
  change_processor()->Put(storage_key, MoveToEntityData(std::move(specifics)),
                          batch->GetMetadataChangeList());

  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&UserEventSyncBridge::OnStoreCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void UserEventSyncBridge::OnStoreLoaded(
    const std::optional<ModelError>& error,
    std::unique_ptr<StoreWithCache> store,
    std::unique_ptr<MetadataBatch> metadata_batch) {
  TRACE_EVENT0("sync", "syncer::UserEventSyncBridge::OnStoreLoaded");

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  CHECK(store_);

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void UserEventSyncBridge::OnStoreCommit(
    const std::optional<ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void UserEventSyncBridge::HandleGlobalIdChange(int64_t old_global_id,
                                               int64_t new_global_id) {
  DCHECK_NE(old_global_id, new_global_id);

  // Store specifics in a temp vector while erasing, as |RecordUserEvent()| will
  // insert new values into |in_flight_nav_linked_events_|. While insert should
  // not invalidate a std::multimap's iterator, and the updated global_id should
  // not be within our given range, this approach seems less error prone.
  std::vector<std::unique_ptr<UserEventSpecifics>> affected;

  auto [begin, end] = in_flight_nav_linked_events_.equal_range(old_global_id);
  for (auto iter = begin; iter != end;) {
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
