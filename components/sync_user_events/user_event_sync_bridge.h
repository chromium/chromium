// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SYNC_BRIDGE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_with_in_memory_cache.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync_user_events/global_id_mapper.h"

namespace syncer {

class UserEventSyncBridge : public DataTypeSyncBridge {
 public:
  UserEventSyncBridge(
      OnceDataTypeStoreFactory store_factory,
      std::unique_ptr<DataTypeLocalChangeProcessor> change_processor,
      GlobalIdMapper* global_id_mapper);

  UserEventSyncBridge(const UserEventSyncBridge&) = delete;
  UserEventSyncBridge& operator=(const UserEventSyncBridge&) = delete;

  ~UserEventSyncBridge() override;

  // DataTypeSyncBridge implementation.
  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override;
  std::optional<ModelError> MergeFullSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_data) override;
  std::optional<ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override;
  std::unique_ptr<DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(const EntityData& entity_data) override;
  std::string GetStorageKey(const EntityData& entity_data) override;
  void ApplyDisableSyncChanges(
      std::unique_ptr<MetadataChangeList> delete_metadata_change_list) override;

  void RecordUserEvent(std::unique_ptr<sync_pb::UserEventSpecifics> specifics);

  static std::string GetStorageKeyFromSpecificsForTest(
      const sync_pb::UserEventSpecifics& specifics);
  std::unique_ptr<DataTypeStore> StealStoreForTest();

 private:
  using StoreWithCache =
      syncer::DataTypeStoreWithInMemoryCache<sync_pb::UserEventSpecifics>;

  void RecordUserEventImpl(
      std::unique_ptr<sync_pb::UserEventSpecifics> specifics);

  void OnStoreLoaded(const std::optional<syncer::ModelError>& error,
                     std::unique_ptr<StoreWithCache> store,
                     std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnStoreCommit(const std::optional<ModelError>& error);

  void HandleGlobalIdChange(int64_t old_global_id, int64_t new_global_id);

  // Persistent storage for in flight events. Should remain quite small, as we
  // delete upon commit confirmation.
  // Null upon construction, until the store is successfully initialized.
  std::unique_ptr<StoreWithCache> store_;

  // The key is the global_id of the navigation the event is linked to.
  std::multimap<int64_t, sync_pb::UserEventSpecifics>
      in_flight_nav_linked_events_;

  const raw_ptr<GlobalIdMapper> global_id_mapper_;

  base::WeakPtrFactory<UserEventSyncBridge> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SYNC_BRIDGE_H_
