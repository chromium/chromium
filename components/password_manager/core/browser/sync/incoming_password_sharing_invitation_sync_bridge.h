// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_INCOMING_PASSWORD_SHARING_INVITATION_SYNC_BRIDGE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_INCOMING_PASSWORD_SHARING_INVITATION_SYNC_BRIDGE_H_

#include <memory>
#include "base/sequence_checker.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class MetadataChangeList;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace password_manager {

// Sync bridge implementation for INCOMING_PASSWORD_SHARING_INVITATION model
// type.
class IncomingPasswordSharingInvitationSyncBridge
    : public syncer::ModelTypeSyncBridge {
 public:
  explicit IncomingPasswordSharingInvitationSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
  IncomingPasswordSharingInvitationSyncBridge(
      const IncomingPasswordSharingInvitationSyncBridge&) = delete;
  IncomingPasswordSharingInvitationSyncBridge& operator=(
      const IncomingPasswordSharingInvitationSyncBridge&) = delete;
  ~IncomingPasswordSharingInvitationSyncBridge() override;

  // ModelTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  absl::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  bool SupportsGetClientTag() const override;
  bool SupportsGetStorageKey() const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_INCOMING_PASSWORD_SHARING_INVITATION_SYNC_BRIDGE_H_
