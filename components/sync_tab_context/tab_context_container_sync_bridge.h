// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_CONTAINER_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_CONTAINER_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/uuid.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/protocol/encrypted_tab_context_container_specifics.pb.h"
#include "components/sync_tab_context/container_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace syncer {
class AgileSymmetricKeySet;
class DataBatch;
class MetadataChangeList;
}  // namespace syncer

namespace sync_tab_context {

class TabContextContainerSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  TabContextContainerSyncBridge(
      syncer::OnceDataTypeStoreFactory store_factory,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);
  TabContextContainerSyncBridge(const TabContextContainerSyncBridge&) = delete;
  TabContextContainerSyncBridge& operator=(
      const TabContextContainerSyncBridge&) = delete;
  ~TabContextContainerSyncBridge() override;

  // Creates and persists a new container with `container_id`, `key_set`, and
  // an optional `pre_minted_access_token`. Requires `IsTrackingMetadata()` to
  // be true and `key_set` to be non-null and non-empty. If
  // `pre_minted_access_token` is provided, it must be non-empty; both `key_set`
  // and `pre_minted_access_token` are persisted in the local store, whereas
  // `key_set` is omitted from the specifics committed to the Sync server.
  void CreateContainer(const ContainerId& container_id,
                       std::unique_ptr<syncer::AgileSymmetricKeySet> key_set,
                       std::optional<std::string> pre_minted_access_token);

  // Returns the stored encryption key for a container or null if no container
  // exists with ID `container_id` (or if it was downloaded remotely without an
  // encryption key).
  const syncer::AgileSymmetricKeySet* GetStoredEncryptionKeyForContainer(
      const ContainerId& container_id) const;

  // Returns the stored pre-minted access token for a container if one is
  // stored, or `std::nullopt` otherwise.
  std::optional<std::string> GetStoredPreMintedAccessTokenForContainer(
      const ContainerId& container_id) const;

  // Returns true if the change processor is tracking metadata (i.e., local data
  // has loaded and sync is ready).
  bool IsTrackingMetadata() const;

  // syncer::DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;

 private:
  struct Entry {
    Entry(std::unique_ptr<syncer::AgileSymmetricKeySet> key_set,
          std::optional<std::string> pre_minted_access_token);
    Entry(Entry&&);
    Entry& operator=(Entry&&);
    ~Entry();

    std::unique_ptr<syncer::AgileSymmetricKeySet> key_set;
    std::optional<std::string> pre_minted_access_token;
  };

  std::unique_ptr<syncer::AgileSymmetricKeySet> ExtractOrPreserveEncryptionKey(
      const ContainerId& container_id,
      const sync_pb::EncryptedTabContextContainerSpecifics& specifics) const;

  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);
  void OnReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnStoreCommit(const std::optional<syncer::ModelError>& error);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<syncer::DataTypeStore> store_;

  absl::flat_hash_map<ContainerId, Entry, ContainerIdHash> entries_;

  base::WeakPtrFactory<TabContextContainerSyncBridge> weak_ptr_factory_{this};
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_CONTAINER_SYNC_BRIDGE_H_
