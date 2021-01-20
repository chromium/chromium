// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SYNC_BRIDGE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync_user_events/global_id_mapper.h"

namespace syncer {

class UserEventSyncBridge : public ModelTypeSyncBridge {
 public:
  UserEventSyncBridge(
      OnceModelTypeStoreFactory store_factory,
      std::unique_ptr<ModelTypeChangeProcessor> change_processor,
      GlobalIdMapper* global_id_mapper);
  ~UserEventSyncBridge() override;

  // ModelTypeSyncBridge implementation.
  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override;
  base::Optional<ModelError> MergeSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_data) override;
  base::Optional<ModelError> ApplySyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const EntityData& entity_data) override;
  std::string GetStorageKey(const EntityData& entity_data) override;
  void ApplyStopSyncChanges(
      std::unique_ptr<MetadataChangeList> delete_metadata_change_list) override;

  void RecordUserEvent(std::unique_ptr<sync_pb::UserEventSpecifics> specifics);

  static std::string GetStorageKeyFromSpecificsForTest(
      const sync_pb::UserEventSpecifics& specifics);
  std::unique_ptr<ModelTypeStore> StealStoreForTest();

 private:
  void RecordUserEventImpl(
      std::unique_ptr<sync_pb::UserEventSpecifics> specifics);

  void OnStoreCreated(const base::Optional<ModelError>& error,
                      std::unique_ptr<ModelTypeStore> store);
  void OnReadAllMetadata(const base::Optional<ModelError>& error,
                         std::unique_ptr<MetadataBatch> metadata_batch);
  void OnCommit(const base::Optional<ModelError>& error);
  void OnReadData(DataCallback callback,
                  const base::Optional<ModelError>& error,
                  std::unique_ptr<ModelTypeStore::RecordList> data_records,
                  std::unique_ptr<ModelTypeStore::IdList> missing_id_list);
  void OnReadAllData(DataCallback callback,
                     const base::Optional<ModelError>& error,
                     std::unique_ptr<ModelTypeStore::RecordList> data_records);

  void HandleGlobalIdChange(int64_t old_global_id, int64_t new_global_id);

  // Persistent storage for in flight events. Should remain quite small, as we
  // delete upon commit confirmation.
  std::unique_ptr<ModelTypeStore> store_;

  // The key is the global_id of the navigation the event is linked to.
  std::multimap<int64_t, sync_pb::UserEventSpecifics>
      in_flight_nav_linked_events_;

  GlobalIdMapper* global_id_mapper_;

  base::WeakPtrFactory<UserEventSyncBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UserEventSyncBridge);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_SYNC_BRIDGE_H_
