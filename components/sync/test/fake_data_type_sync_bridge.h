// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_SYNC_BRIDGE_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>

#include "base/functional/callback_forward.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/unique_position.pb.h"

namespace sync_pb {
class EntityMetadata;
class EntitySpecifics;
}  // namespace sync_pb

namespace syncer {

class MetadataBatch;
struct EntityData;

// A basic, functional implementation of DataTypeSyncBridge for testing
// purposes. It uses its own simple in-memory Store class.
class FakeDataTypeSyncBridge : public DataTypeSyncBridge {
 public:
  using ExtractUniquePositionCallback =
      base::RepeatingCallback<sync_pb::UniquePosition(
          const sync_pb::EntitySpecifics& specifics)>;

  // Generate a client tag with the given key.
  static std::string ClientTagFromKey(const std::string& key);

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
    const sync_pb::EntityMetadata& GetMetadata(const std::string& key) const;

    const std::map<std::string, std::unique_ptr<EntityData>>& all_data() const {
      return data_store_;
    }

    size_t data_count() const { return data_store_.size(); }
    size_t metadata_count() const { return metadata_store_.size(); }
    size_t data_change_count() const { return data_change_count_; }
    size_t metadata_change_count() const { return metadata_change_count_; }

    const sync_pb::DataTypeState& data_type_state() const {
      return data_type_state_;
    }

    void set_data_type_state(const sync_pb::DataTypeState& data_type_state) {
      data_type_state_ = data_type_state;
    }

    std::unique_ptr<MetadataBatch> CreateMetadataBatch() const;
    void Reset();

   private:
    size_t data_change_count_ = 0;
    size_t metadata_change_count_ = 0;
    std::map<std::string, std::unique_ptr<EntityData>> data_store_;
    std::map<std::string, sync_pb::EntityMetadata> metadata_store_;
    sync_pb::DataTypeState data_type_state_;
  };

  FakeDataTypeSyncBridge(
      DataType type,
      std::unique_ptr<DataTypeLocalChangeProcessor> change_processor);
  ~FakeDataTypeSyncBridge() override;

  DataType type() { return type_; }

  // Local data modification. Emulates signals from the model thread.
  void WriteItem(const std::string& key,
                 std::unique_ptr<EntityData> entity_data);

  // Local data deletion.
  void DeleteItem(const std::string& key);

  // Deletes local data without notifying the processor (useful for modeling
  // faulty bridges).
  void MimicBugToLooseItemWithoutNotifyingProcessor(const std::string& key);

  // DataTypeSyncBridge implementation
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
  bool SupportsGetClientTag() const override;
  bool SupportsGetStorageKey() const override;
  bool SupportsUniquePositions() const override;
  sync_pb::UniquePosition GetUniquePosition(
      const sync_pb::EntitySpecifics& specifics) const override;
  ConflictResolution ResolveConflict(
      const std::string& storage_key,
      const EntityData& remote_data) const override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;
  bool IsEntityDataValid(const EntityData& entity_data) const override;

  // Stores a resolution for the next call to ResolveConflict. Note that if this
  // is a USE_NEW resolution, the data will only exist for one resolve call.
  void SetConflictResolution(ConflictResolution resolution);

  // Sets an error that the next fallible call to the bridge will generate.
  void ErrorOnNextCall();

  // It is intentionally very difficult to copy an EntityData, as in normal code
  // we never want to. However, since we store the data as an EntityData for the
  // test code here, this function is needed to manually copy it.
  static std::unique_ptr<EntityData> CopyEntityData(const EntityData& old_data);

  // Influences the way the bridge provides client tags. If set to true, the
  // bridge will compute a client tag deterministically from specifics, via
  // GetClientTag(). If set to false, bridge is responsible for setting client
  // tag hashes in EntityData whenever committing entities.
  void SetSupportsGetClientTag(bool supports_get_client_tag);

  // Checks whether |entity| has the client tag hash filled in or whether the
  // bridge itself is able to provide the client tag to the processor.
  bool EntityHasClientTag(const EntityData& entity);

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

  // Add preference values that will be ignored by bridge. Must only be called
  // if the bridge's DataType is PREFERENCES.
  void AddPrefValueToIgnore(const std::string& value);

  // Sets the flag to mark entities with client tag hash `client_tag_hash` as
  // invalid when IsEntityDataValid() is called.
  void TreatRemoteUpdateAsInvalid(const ClientTagHash& client_tag_hash);

  // Enables unique position support. The `callback` is used to extract unique
  // position from specifics.
  void EnableUniquePositionSupport(ExtractUniquePositionCallback callback);

  // Storage keys for the entities with deleted collaboration membership.
  const std::set<std::string>& deleted_collaboration_membership_storage_keys()
      const {
    return deleted_collaboration_membership_storage_keys_;
  }

  const Store& db() const { return *db_; }
  Store* mutable_db() { return db_.get(); }
  size_t trimmed_specifics_change_count() const {
    return trimmed_specifics_change_count_;
  }

 protected:
  // Contains all of the data and metadata state.
  std::unique_ptr<Store> db_;

 private:
  // Applies |change_list| to the metadata store.
  void ApplyMetadataChangeList(std::unique_ptr<MetadataChangeList> change_list);

  // Same as GetStorageKey(), but doesn't check that SupportsGetStorageKey()
  // is true.
  std::string GetStorageKeyInternal(const EntityData& entity_data);

  // If SupportsGetStorageKey() is true, same as GetStorageKey(). Otherwise,
  // generates and returns a new unique storage key.
  std::string GenerateStorageKey(const EntityData& entity_data);

  const DataType type_;

  // The conflict resolution to use for calls to ResolveConflict.
  ConflictResolution conflict_resolution_;

  // The preference values that the bridge will ignore.
  std::unordered_set<std::string> values_to_ignore_;

  // The client tag hashes the bridge will mark as invalid in
  // calls to IsEntityDataValid().
  std::set<ClientTagHash> invalid_remote_updates_;

  // Whether an error should be produced on the next bridge call.
  bool error_next_ = false;

  // Whether the bridge supports call to GetStorageKey. If it doesn't, bridge is
  // responsible for calling UpdateStorageKey when processing new entities in
  // MergeFullSyncData/ApplyIncrementalSyncChanges.
  bool supports_get_storage_key_ = true;

  // Whether the bridge supports call to GetClientTag. If it doesn't, bridge is
  // responsible for setting client tag hashes in EntityData whenever committing
  // entities.
  bool supports_get_client_tag_ = true;

  // Last dynamically-generated storage key, for the case where
  // |supports_get_storage_key_| == false (otherwise the storage key gets
  // inferred deterministically from specifics).
  int last_generated_storage_key_ = 0;

  mutable size_t trimmed_specifics_change_count_ = 0;

  std::set<std::string> deleted_collaboration_membership_storage_keys_;

  ExtractUniquePositionCallback extract_unique_positions_callback_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_SYNC_BRIDGE_H_
