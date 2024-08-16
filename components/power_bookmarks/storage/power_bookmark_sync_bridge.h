// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_SYNC_BRIDGE_H_
#define COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_SYNC_BRIDGE_H_

#include "base/uuid.h"
#include "components/sync/model/data_type_sync_bridge.h"

namespace syncer {
class ModelError;
}  // namespace syncer

namespace power_bookmarks {

class Power;
class PowerBookmarkSyncMetadataDatabase;

// Transaction wraps a database transaction. When it's out of scope the
// underlying transaction will be cancelled if not committed.
// TODO(crbug.com/40247772): Find a better layout for this class.
class Transaction {
 public:
  virtual bool Commit() = 0;
  virtual ~Transaction() = default;
};

// PowerBookmarkSyncBridge is responsible for syncing all powers to different
// devices. It runs on the same thread as the power bookmark database
// implementation.
class PowerBookmarkSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  // Delegate interface PowerBookmarkSyncBridge needs from the backend.
  class Delegate {
   public:
    // Get all the powers from the database.
    virtual std::vector<std::unique_ptr<Power>> GetAllPowers() = 0;

    // Get powers for the given guids.
    virtual std::vector<std::unique_ptr<Power>> GetPowersForGUIDs(
        const std::vector<std::string>& guids) = 0;

    // Get power for the given guid.
    virtual std::unique_ptr<Power> GetPowerForGUID(const std::string& guid) = 0;

    // Create a power if not exists or merge existing the power in the database.
    virtual bool CreateOrMergePowerFromSync(const Power& power) = 0;

    // Delete a power from the database.
    virtual bool DeletePowerFromSync(const std::string& guid) = 0;

    // Get the database to store power bookmarks metadata.
    virtual PowerBookmarkSyncMetadataDatabase* GetSyncMetadataDatabase() = 0;

    // Start a transaction. This is used to make sure power bookmark data
    // and metadata are stored atomically.
    virtual std::unique_ptr<Transaction> BeginTransaction() = 0;

    // Notify the backend if powers are changed.
    virtual void NotifyPowersChanged() {}
  };

  PowerBookmarkSyncBridge(
      PowerBookmarkSyncMetadataDatabase* meta_db,
      Delegate* delegate,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);

  PowerBookmarkSyncBridge(const PowerBookmarkSyncBridge&) = delete;
  PowerBookmarkSyncBridge& operator=(const PowerBookmarkSyncBridge&) = delete;

  ~PowerBookmarkSyncBridge() override;

  void Init();

  // syncer::DataTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;

  void SendPowerToSync(const Power& power);
  void NotifySyncForDeletion(const std::string& guid);

  void ReportError(const syncer::ModelError& error);
  bool initialized() { return initialized_; }

 private:
  // Create a change list to store metadata inside the power bookmark database.
  // This method should be called inside a transaction because Chrome sync
  // requires saving data and metadata atomically. Also need to transfer the
  // meta_data_change_list from the InMemoryMetadataChangeList created by
  // CreateMetadataChangeList() within the transaction created in
  // MergeFullSyncData() and ApplyIncrementalSyncChanges().
  std::unique_ptr<syncer::MetadataChangeList>
  CreateMetadataChangeListInTransaction();

  // Helper function called by both `MergeFullSyncData` with
  // is_initial_merge=true and `ApplyIncrementalSyncChanges` with
  // is_initial_merge=false.
  std::optional<syncer::ModelError> ApplyChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList& entity_changes,
      bool is_initial_merge);

  const raw_ptr<PowerBookmarkSyncMetadataDatabase, DanglingUntriaged> meta_db_;
  const raw_ptr<Delegate> delegate_;
  bool initialized_ = false;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_SYNC_BRIDGE_H_
