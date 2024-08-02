// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_OUTGOING_PASSWORD_SHARING_INVITATION_SYNC_BRIDGE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_OUTGOING_PASSWORD_SHARING_INVITATION_SYNC_BRIDGE_H_

#include <map>
#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "components/password_manager/core/browser/sharing/recipient_info.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
class MetadataChangeList;
}  // namespace syncer

namespace password_manager {

struct PasswordForm;
struct PasswordRecipient;

// Sync bridge implementation for OUTGOING_PASSWORD_SHARING_INVITATION model
// type.
class OutgoingPasswordSharingInvitationSyncBridge
    : public syncer::DataTypeSyncBridge {
 public:
  explicit OutgoingPasswordSharingInvitationSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);
  OutgoingPasswordSharingInvitationSyncBridge(
      const OutgoingPasswordSharingInvitationSyncBridge&) = delete;
  OutgoingPasswordSharingInvitationSyncBridge& operator=(
      const OutgoingPasswordSharingInvitationSyncBridge&) = delete;
  ~OutgoingPasswordSharingInvitationSyncBridge() override;

  // Sends `passwords` to the corresponding `recipient`. All entries in
  // `passwords` are expected to belong to the same credentials group. i.e. they
  // all share the same username and password, and all origins are affiliated.
  // `passwords` must not be empty.
  void SendPasswordGroup(const std::vector<PasswordForm>& passwords,
                         const PasswordRecipient& recipient);

  // DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  bool SupportsGetClientTag() const override;
  bool SupportsGetStorageKey() const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  void OnCommitAttemptErrors(
      const syncer::FailedCommitResponseDataList& error_response_list) override;
  CommitAttemptFailedBehavior OnCommitAttemptFailed(
      syncer::SyncCommitError commit_error) override;

  static syncer::ClientTagHash GetClientTagHashFromStorageKeyForTest(
      const std::string& storage_key);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Contains data which is sufficient for creating `EntityData` for an outgoing
  // invitation.
  struct OutgoingInvitationWithEncryptionKey {
    sync_pb::OutgoingPasswordSharingInvitationSpecifics specifics;
    PublicKey recipient_public_key;
  };

  static std::unique_ptr<syncer::EntityData> ConvertToEntityData(
      const OutgoingInvitationWithEncryptionKey&
          invitation_with_encryption_key);

  // Last sent passwords are cached until they are committed to the server. This
  // is required to keep data in case of retries.
  std::map<syncer::ClientTagHash, OutgoingInvitationWithEncryptionKey>
      outgoing_invitations_in_flight_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_OUTGOING_PASSWORD_SHARING_INVITATION_SYNC_BRIDGE_H_
