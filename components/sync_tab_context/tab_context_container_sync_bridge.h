// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_CONTAINER_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_CONTAINER_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/sequence_checker.h"
#include "base/uuid.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/protocol/encrypted_tab_context_container_specifics.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace syncer {
class DataBatch;
class MetadataChangeList;
}  // namespace syncer

namespace sync_tab_context {

class TabContextContainerSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  explicit TabContextContainerSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);
  TabContextContainerSyncBridge(const TabContextContainerSyncBridge&) = delete;
  TabContextContainerSyncBridge& operator=(
      const TabContextContainerSyncBridge&) = delete;
  ~TabContextContainerSyncBridge() override;

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

  absl::flat_hash_map<base::Uuid,
                      sync_pb::EncryptedTabContextContainerSpecifics,
                      base::UuidHash>
      entries_;
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_CONTAINER_SYNC_BRIDGE_H_
