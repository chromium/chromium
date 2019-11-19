// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_FAKE_MODEL_TYPE_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_MODEL_FAKE_MODEL_TYPE_SYNC_BRIDGE_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_set>

#include "base/optional.h"
#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

class ClientTagHash;

// A basic, functional implementation of ModelTypeSyncBridge for testing
// purposes. It uses the PREFERENCES type to provide a simple key/value
// interface, and uses its own simple in-memory Store class.
class FakeModelTypeSyncBridge : public ModelTypeSyncBridge {
 public:
  // Generate a client tag with the given key.
  static std::string ClientTagFromKey(const std::string& key);

  // Generates the tag hash for a given key.
  static ClientTagHash TagHashFromKey(const std::string& key);

  // Generates entity specifics for the given key and value.
  static sync_pb::EntitySpecifics GenerateSpecifics(const std::string& key,
                                                    const std::string& value);

  // Generates an EntityData for the given key and value.
  static std::unique_ptr<EntityData> GenerateEntityData(
      const std::string& key,
      const std::string& value);

  // A basic in-memory storage mechanism for data and metadata. This makes it
  // easier to test more complex behaviors involving when entities are written,
  // committed, etc. Having a separate class helps keep the main one cleaner.
  class Store {
   public:
    Store();
    virtual ~Store();

    void PutData(const std::string& key, const EntityData& data);
    void PutMetadata(const std::string& key,
                     const sync_pb::EntityMetadata& metadata);
    void RemoveData(const std::string& key);
    void ClearAllData();
    void RemoveMetadata(const std::string& key);
    bool HasData(const std::string& key) const;
    bool HasMetadata(const std::string& key) const;
    const EntityData& GetData(const std::string& key) const;
    const std::string& GetValue(const std::string& key) const;
    const sync_pb::EntityMetadata& GetMetadata(const std::string& key) const;

    const std::map<std::string, std::unique_ptr<EntityData>>& all_data() const {
      return data_store_;
    }

    size_t data_count() const { return data_store_.size(); }
    size_t metadata_count() const { return metadata_store_.size(); }
    size_t data_change_count() const { return data_change_count_; }
    size_t metadata_change_count() const { return metadata_change_count_; }

    const sync_pb::ModelTypeState& model_type_state() const {
      return model_type_state_;
    }

    void set_model_type_state(const sync_pb::ModelTypeState& model_type_state) {
      model_type_state_ = model_type_state;
    }

    std::unique_ptr<MetadataBatch> CreateMetadataBatch() const;
    void Reset();

   private:
    size_t data_change_count_ = 0;
    size_t metadata_change_count_ = 0;
    std::map<std::string, std::unique_ptr<EntityData>> data_store_;
    std::map<std::string, sync_pb::EntityMetadata> metadata_store_;
    sync_pb::ModelTypeState model_type_state_;
  };

  explicit FakeModelTypeSyncBridge(
      std::unique_ptr<ModelTypeChangeProcessor> change_processor);
  ~FakeModelTypeSyncBridge() override;

  // Local data modification. Emulates signals from the model thread.
  sync_pb::EntitySpecifics WriteItem(const std::string& key,
                                     const std::string& value);

  // Overloaded form to allow passing of custom entity data.
  void WriteItem(const std::string& key,
                 std::unique_ptr<EntityData> entity_data);

  // Local data deletion.
  void DeleteItem(const std::string& key);

  // Deletes local data without notifying the processor (useful for modeling
  // faulty bridges).
  void MimicBugToLooseItemWithoutNotifyingProcessor(const std::string& key);

  // ModelTypeSyncBridge implementation
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
  bool SupportsGetStorageKey() const override;
  ConflictResolution ResolveConflict(
      const std::string& storage_key,
      const EntityData& remote_data) const override;
  void ApplyStopSyncChanges(
      std::unique_ptr<MetadataChangeList> delete_metadata_change_list) override;

  // Stores a resolution for the next call to ResolveConflict. Note that if this
  // is a USE_NEW resolution, the data will only exist for one resolve call.
  void SetConflictResolution(ConflictResolution resolution);

  // Sets an error that the next fallible call to the bridge will generate.
  void ErrorOnNextCall();

  // It is intentionally very difficult to copy an EntityData, as in normal code
  // we never want to. However, since we store the data as an EntityData for the
  // test code here, this function is needed to manually copy it.
  static std::unique_ptr<EntityData> CopyEntityData(const EntityData& old_data);

  // Influences the way the bridge produces storage key. If set to true, the
  // bridge will compute a storage key deterministically from specifics, via
  // GetStorageKey(). If set to false, it will return autoincrement-like storage
  // keys that cannot be inferred from specifics, and exercise
  // UpdateStorageKey() for remote changes to report storage keys.
  void SetSupportsGetStorageKey(bool supports_get_storage_key);

  // Returns the last generated autoincrement-like storage key, applicable only
  // for the SetSupportsGetStorageKey(false) case (otherwise the storage key
  // gets inferred deterministically from specifics).
  std::string GetLastGeneratedStorageKey() const;

  // Add values that will be ignored by bridge.
  void AddValueToIgnore(const std::string& value);

  const Store& db() const { return *db_; }
  Store* mutable_db() { return db_.get(); }

 protected:
  // Contains all of the data and metadata state.
  std::unique_ptr<Store> db_;

 private:
  // Applies |change_list| to the metadata store.
  void ApplyMetadataChangeList(std::unique_ptr<MetadataChangeList> change_list);

  std::string GenerateStorageKey(const EntityData& entity_data);

  // The conflict resolution to use for calls to ResolveConflict.
  ConflictResolution conflict_resolution_;

  // The keys that the bridge will ignore.
  std::unordered_set<std::string> values_to_ignore_;

  // Whether an error should be produced on the next bridge call.
  bool error_next_ = false;

  // Whether the bridge supports call to GetStorageKey. If it doesn't bridge is
  // responsible for calling UpdateStorageKey when processing new entities in
  // MergeSyncData/ApplySyncChanges.
  bool supports_get_storage_key_ = true;

  // Last dynamically-generated storage key, for the case where
  // |supports_get_storage_key_| == false (otherwise the storage key gets
  // inferred deterministically from specifics).
  int last_generated_storage_key_ = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_FAKE_MODEL_TYPE_SYNC_BRIDGE_H_
