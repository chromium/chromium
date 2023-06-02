// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/outgoing_password_sharing_invitation_sync_bridge.h"

#include "components/sync/model/dummy_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"

namespace password_manager {

OutgoingPasswordSharingInvitationSyncBridge::
    OutgoingPasswordSharingInvitationSyncBridge(
        std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {
  // Current data type doesn't have persistent storage so it's ready to sync
  // immediately.
  this->change_processor()->ModelReadyToSync(
      std::make_unique<syncer::MetadataBatch>());
}

OutgoingPasswordSharingInvitationSyncBridge::
    ~OutgoingPasswordSharingInvitationSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
OutgoingPasswordSharingInvitationSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The data type intentionally doesn't persist the data on disk, so metadata
  // is just ignored.
  return std::make_unique<syncer::DummyMetadataChangeList>();
}

absl::optional<syncer::ModelError>
OutgoingPasswordSharingInvitationSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entity_data.empty());
  return absl::nullopt;
}

absl::optional<syncer::ModelError>
OutgoingPasswordSharingInvitationSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return absl::nullopt;
}

void OutgoingPasswordSharingInvitationSyncBridge::GetData(
    StorageKeyList storage_keys,
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  std::move(callback).Run(std::move(batch));
}

void OutgoingPasswordSharingInvitationSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  std::move(callback).Run(std::move(batch));
}

std::string OutgoingPasswordSharingInvitationSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string OutgoingPasswordSharingInvitationSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.outgoing_password_sharing_invitation().guid();
}

bool OutgoingPasswordSharingInvitationSyncBridge::SupportsGetClientTag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

bool OutgoingPasswordSharingInvitationSyncBridge::SupportsGetStorageKey()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void OutgoingPasswordSharingInvitationSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

sync_pb::EntitySpecifics OutgoingPasswordSharingInvitationSyncBridge::
    TrimAllSupportedFieldsFromRemoteSpecifics(
        const sync_pb::EntitySpecifics& entity_specifics) const {
  NOTIMPLEMENTED();
  return ModelTypeSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
      entity_specifics);
}

}  // namespace password_manager
