// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"

#include "base/functional/callback_helpers.h"
#include "base/uuid.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sharing/password_receiver_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"

namespace password_manager {

IncomingPasswordSharingInvitationSyncBridge::
    IncomingPasswordSharingInvitationSyncBridge(
        std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
        syncer::OnceDataTypeStoreFactory create_sync_metadata_store_callback)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  std::move(create_sync_metadata_store_callback)
      .Run(syncer::INCOMING_PASSWORD_SHARING_INVITATION,
           base::BindOnce(&IncomingPasswordSharingInvitationSyncBridge::
                              OnDataTypeStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

IncomingPasswordSharingInvitationSyncBridge::
    ~IncomingPasswordSharingInvitationSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IncomingPasswordSharingInvitationSyncBridge::SetPasswordReceiverService(
    PasswordReceiverService* password_receiver_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(password_receiver_service);
  password_receiver_service_ = password_receiver_service;
}

void IncomingPasswordSharingInvitationSyncBridge::OnSyncStarting(
    const syncer::DataTypeActivationRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Verify that |password_receiver_service_| has been provided before
  // interacting with the server.
  CHECK(password_receiver_service_);
}

std::unique_ptr<syncer::MetadataChangeList>
IncomingPasswordSharingInvitationSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError>
IncomingPasswordSharingInvitationSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // There is no local data stored for the data type, hence nothing to merge.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<syncer::ModelError>
IncomingPasswordSharingInvitationSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(password_receiver_service_);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      sync_metadata_store_->CreateWriteBatch();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      // The bridge does not store any data hence just ignore incoming
      // deletions.
      continue;
    }

    password_receiver_service_->ProcessIncomingSharingInvitation(
        change->data().specifics.incoming_password_sharing_invitation());

    // After the invitation has been processed, delete it from the server, so
    // that no other client will process it.
    change_processor()->Delete(change->storage_key(),
                               syncer::DeletionOrigin::Unspecified(),
                               metadata_change_list.get());
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  CommitSyncMetadata(std::move(batch));
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch>
IncomingPasswordSharingInvitationSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTREACHED_IN_MIGRATION()
      << "This data type does not store or commit any data to the server.";
  return nullptr;
}

std::unique_ptr<syncer::DataBatch>
IncomingPasswordSharingInvitationSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // There is no data stored locally, return an empty result.
  // TODO(crbug.com/40268334): return at least sync metadata if available. This
  // requires a storage key list.
  return std::make_unique<syncer::MutableDataBatch>();
}

std::string IncomingPasswordSharingInvitationSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.incoming_password_sharing_invitation().guid();
}

std::string IncomingPasswordSharingInvitationSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return GetClientTag(entity_data);
}

bool IncomingPasswordSharingInvitationSyncBridge::SupportsGetClientTag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

bool IncomingPasswordSharingInvitationSyncBridge::SupportsGetStorageKey()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void IncomingPasswordSharingInvitationSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(sync_metadata_store_);
  sync_metadata_store_->DeleteAllDataAndMetadata(base::DoNothing());
}

bool IncomingPasswordSharingInvitationSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  // Verify that the invitation has some basic correct fields. All the other
  // fields related to password data is verified by PasswordReceiverService.
  const sync_pb::IncomingPasswordSharingInvitationSpecifics& specifics =
      entity_data.specifics.incoming_password_sharing_invitation();
  return base::Uuid::ParseLowercase(specifics.guid()).is_valid() &&
         !specifics.sender_info().user_display_info().email().empty() &&
         specifics.has_client_only_unencrypted_data();
}

void IncomingPasswordSharingInvitationSyncBridge::OnDataTypeStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  sync_metadata_store_ = std::move(store);

  sync_metadata_store_->ReadAllMetadata(base::BindOnce(
      &IncomingPasswordSharingInvitationSyncBridge::OnReadAllMetadata,
      weak_ptr_factory_.GetWeakPtr()));
}

void IncomingPasswordSharingInvitationSyncBridge::OnReadAllMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void IncomingPasswordSharingInvitationSyncBridge::OnCommitSyncMetadata(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void IncomingPasswordSharingInvitationSyncBridge::CommitSyncMetadata(
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch) {
  sync_metadata_store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(
          &IncomingPasswordSharingInvitationSyncBridge::OnCommitSyncMetadata,
          weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace password_manager
