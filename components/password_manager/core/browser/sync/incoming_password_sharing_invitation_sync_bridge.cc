// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/incoming_password_sharing_invitation_sync_bridge.h"

#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"

namespace password_manager {

IncomingPasswordSharingInvitationSyncBridge::
    IncomingPasswordSharingInvitationSyncBridge(
        std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {
  // TODO(crbug.com/1445868): read metadata from the store.
  std::unique_ptr<syncer::MetadataBatch> batch =
      std::make_unique<syncer::MetadataBatch>();
  this->change_processor()->ModelReadyToSync(std::move(batch));
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
  NOTIMPLEMENTED();
  return absl::nullopt;
}

absl::optional<syncer::ModelError>
IncomingPasswordSharingInvitationSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return absl::nullopt;
}

void IncomingPasswordSharingInvitationSyncBridge::GetData(
    StorageKeyList storage_keys,
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  std::move(callback).Run(std::move(batch));
}

void IncomingPasswordSharingInvitationSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  std::move(callback).Run(std::move(batch));
}

std::string IncomingPasswordSharingInvitationSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  NOTREACHED();
  return std::string();
}

std::string IncomingPasswordSharingInvitationSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return std::string();
}

bool IncomingPasswordSharingInvitationSyncBridge::SupportsGetClientTag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

bool IncomingPasswordSharingInvitationSyncBridge::SupportsGetStorageKey()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void IncomingPasswordSharingInvitationSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

sync_pb::EntitySpecifics IncomingPasswordSharingInvitationSyncBridge::
    TrimAllSupportedFieldsFromRemoteSpecifics(
        const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return ModelTypeSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
      entity_specifics);
}

}  // namespace password_manager
