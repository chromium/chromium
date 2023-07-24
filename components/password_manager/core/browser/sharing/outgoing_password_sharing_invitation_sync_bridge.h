// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_OUTGOING_PASSWORD_SHARING_INVITATION_SYNC_BRIDGE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_OUTGOING_PASSWORD_SHARING_INVITATION_SYNC_BRIDGE_H_

#include <map>
#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"

namespace syncer {
class MetadataChangeList;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace password_manager {

struct PasswordForm;
struct PasswordRecipient;

// Sync bridge implementation for OUTGOING_PASSWORD_SHARING_INVITATION model
// type.
class OutgoingPasswordSharingInvitationSyncBridge
    : public syncer::ModelTypeSyncBridge {
 public:
  explicit OutgoingPasswordSharingInvitationSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
  OutgoingPasswordSharingInvitationSyncBridge(
      const OutgoingPasswordSharingInvitationSyncBridge&) = delete;
  OutgoingPasswordSharingInvitationSyncBridge& operator=(
      const OutgoingPasswordSharingInvitationSyncBridge&) = delete;
  ~OutgoingPasswordSharingInvitationSyncBridge() override;

  // Sends `password` to the corresponding `recipient`.
  void SendPassword(const PasswordForm& password,
                    const PasswordRecipient& recipient);

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

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Last sent passwords are cached until they are committed to the server. This
  // is required to keep data in case of retries.
  std::map<std::string, sync_pb::OutgoingPasswordSharingInvitationSpecifics>
      storage_key_to_outgoing_invitations_in_flight_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_OUTGOING_PASSWORD_SHARING_INVITATION_SYNC_BRIDGE_H_
