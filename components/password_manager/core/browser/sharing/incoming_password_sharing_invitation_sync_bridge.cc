// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"

#include "base/functional/callback_helpers.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"

namespace password_manager {

IncomingPasswordSharingInvitationSyncBridge::
    IncomingPasswordSharingInvitationSyncBridge(
        std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
        syncer::OnceModelTypeStoreFactory create_sync_metadata_store_callback)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {
  std::move(create_sync_metadata_store_callback)
      .Run(syncer::INCOMING_PASSWORD_SHARING_INVITATION,
           base::BindOnce(&IncomingPasswordSharingInvitationSyncBridge::
                              OnModelTypeStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

IncomingPasswordSharingInvitationSyncBridge::
    ~IncomingPasswordSharingInvitationSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
IncomingPasswordSharingInvitationSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

absl::optional<syncer::ModelError>
IncomingPasswordSharingInvitationSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // There is no local data stored for the data type, hence nothing to merge.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

absl::optional<syncer::ModelError>
IncomingPasswordSharingInvitationSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1445868): Process incoming invitations.
  NOTIMPLEMENTED();
  return absl::nullopt;
}

void IncomingPasswordSharingInvitationSyncBridge::GetData(
    StorageKeyList storage_keys,
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NOTREACHED() << "This data type does not store or commit any data.";
}

void IncomingPasswordSharingInvitationSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // There is no data stored locally, return an empty result.
  // TODO(crbug.com/1445868): return at least sync metadata if available. This
  // requires a storage key list.
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  std::move(callback).Run(std::move(batch));
}

std::string IncomingPasswordSharingInvitationSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.incoming_password_sharing_invitation().guid();
}

std::string IncomingPasswordSharingInvitationSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTREACHED();
  return std::string();
}

bool IncomingPasswordSharingInvitationSyncBridge::SupportsGetClientTag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

bool IncomingPasswordSharingInvitationSyncBridge::SupportsGetStorageKey()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

void IncomingPasswordSharingInvitationSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(sync_metadata_store_);
  sync_metadata_store_->DeleteAllDataAndMetadata(base::DoNothing());
}

void IncomingPasswordSharingInvitationSyncBridge::OnModelTypeStoreCreated(
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
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
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

}  // namespace password_manager
