// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_sync_bridge.h"

#include <utility>

#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"

namespace syncer {

ModelTypeSyncBridge::ModelTypeSyncBridge(
    std::unique_ptr<ModelTypeChangeProcessor> change_processor)
    : change_processor_(std::move(change_processor)) {
  DCHECK(change_processor_);
  change_processor_->OnModelStarting(this);
}

ModelTypeSyncBridge::~ModelTypeSyncBridge() = default;

void ModelTypeSyncBridge::OnSyncStarting(
    const DataTypeActivationRequest& request) {}

bool ModelTypeSyncBridge::SupportsGetClientTag() const {
  return true;
}

bool ModelTypeSyncBridge::SupportsGetStorageKey() const {
  return true;
}

bool ModelTypeSyncBridge::SupportsIncrementalUpdates() const {
  return true;
}

ConflictResolution ModelTypeSyncBridge::ResolveConflict(
    const std::string& storage_key,
    const EntityData& remote_data) const {
  if (remote_data.is_deleted()) {
    return ConflictResolution::kUseLocal;
  }
  return ConflictResolution::kUseRemote;
}

void ModelTypeSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  // Nothing to do if this fails, so just ignore the error it might return.
  ApplyIncrementalSyncChanges(std::move(delete_metadata_change_list),
                              EntityChangeList());
}

void ModelTypeSyncBridge::OnCommitAttemptErrors(
    const syncer::FailedCommitResponseDataList& error_response_list) {
  // By default the bridge just ignores failed commit items.
}

void ModelTypeSyncBridge::OnSyncPaused() {}

ModelTypeSyncBridge::CommitAttemptFailedBehavior
ModelTypeSyncBridge::OnCommitAttemptFailed(SyncCommitError commit_error) {
  return CommitAttemptFailedBehavior::kDontRetryOnNextCycle;
}

size_t ModelTypeSyncBridge::EstimateSyncOverheadMemoryUsage() const {
  return 0U;
}

sync_pb::EntitySpecifics
ModelTypeSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  // Clears all fields by default to avoid the memory and I/O overhead of an
  // additional copy of the data.
  return sync_pb::EntitySpecifics();
}

bool ModelTypeSyncBridge::IsEntityDataValid(
    const EntityData& entity_data) const {
  return true;
}

ModelTypeChangeProcessor* ModelTypeSyncBridge::change_processor() {
  return change_processor_.get();
}

const ModelTypeChangeProcessor* ModelTypeSyncBridge::change_processor() const {
  return change_processor_.get();
}

}  // namespace syncer
