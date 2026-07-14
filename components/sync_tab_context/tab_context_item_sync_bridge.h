// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_ITEM_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_ITEM_SYNC_BRIDGE_H_

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/protocol/encrypted_tab_context_item_specifics.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync_tab_context/container_id.h"

namespace syncer {
class DataBatch;
class MetadataChangeList;
}  // namespace syncer

namespace sync_tab_context {

// Sync bridge that implements the commit-only datatype
// ENCRYPTED_TAB_CONTEXT_ITEM responsible for uploading individual encrypted
// blobs. Note that this data type uses custom encryption (via container
// encryption keys) rather than sync's built-in encryption infrastructure
// (Nigori).
class TabContextItemSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  explicit TabContextItemSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);
  TabContextItemSyncBridge(const TabContextItemSyncBridge&) = delete;
  TabContextItemSyncBridge& operator=(const TabContextItemSyncBridge&) = delete;
  ~TabContextItemSyncBridge() override;

  // Uploads an item into the sync processor for commit.
  bool UploadItem(const ContainerId& container_id,
                  const std::string& item_id,
                  sync_pb::EncryptedData encrypted_data);

  // syncer::DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
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
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_ITEM_SYNC_BRIDGE_H_
