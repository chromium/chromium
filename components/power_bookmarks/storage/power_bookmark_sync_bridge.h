// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_SYNC_BRIDGE_H_
#define COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_SYNC_BRIDGE_H_

#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class ModelError;
}  // namespace syncer

namespace power_bookmarks {

class Power;
class PowerBookmarkSyncMetadataDatabase;

// PowerBookmarkSyncBridge is responsible for syncing all powers to different
// devices. It runs on the same thread as the power bookmark database
// implementation.
class PowerBookmarkSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  class Delegate {
   public:
    virtual std::vector<std::unique_ptr<Power>> GetAllPowers() = 0;
    virtual std::vector<std::unique_ptr<Power>> GetPowersForGUIDs(
        const std::vector<std::string>& guids) = 0;
    virtual std::unique_ptr<Power> GetPowerForGUID(const std::string& guid) = 0;
  };
  PowerBookmarkSyncBridge(
      PowerBookmarkSyncMetadataDatabase* meta_db,
      Delegate* delegate,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);

  PowerBookmarkSyncBridge(const PowerBookmarkSyncBridge&) = delete;
  PowerBookmarkSyncBridge& operator=(const PowerBookmarkSyncBridge&) = delete;

  ~PowerBookmarkSyncBridge() override;

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  absl::optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;

 private:
  const raw_ptr<PowerBookmarkSyncMetadataDatabase> meta_db_;
  const raw_ptr<Delegate> delegate_;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_SYNC_BRIDGE_H_
