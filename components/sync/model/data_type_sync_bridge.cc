// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/data_type_sync_bridge.h"

#include <utility>

#include "base/notreached.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"

namespace syncer {

DataTypeSyncBridge::DataTypeSyncBridge(
    std::unique_ptr<DataTypeLocalChangeProcessor> change_processor)
    : change_processor_(std::move(change_processor)) {
  DCHECK(change_processor_);
  change_processor_->OnModelStarting(this);
}

DataTypeSyncBridge::~DataTypeSyncBridge() = default;

void DataTypeSyncBridge::OnSyncStarting(
    const DataTypeActivationRequest& request) {}

bool DataTypeSyncBridge::SupportsGetClientTag() const {
  return true;
}

bool DataTypeSyncBridge::SupportsGetStorageKey() const {
  return true;
}

bool DataTypeSyncBridge::SupportsIncrementalUpdates() const {
  return true;
}

bool DataTypeSyncBridge::SupportsUniquePositions() const {
  return false;
}

sync_pb::UniquePosition DataTypeSyncBridge::GetUniquePosition(
    const sync_pb::EntitySpecifics& specifics) const {
  CHECK(SupportsUniquePositions());
  NOTREACHED()
      << "GetUniquePosition() must be implemented to support unique positions.";
}

ConflictResolution DataTypeSyncBridge::ResolveConflict(
    const std::string& storage_key,
    const EntityData& remote_data) const {
  if (remote_data.is_deleted()) {
    return ConflictResolution::kUseLocal;
  }
  return ConflictResolution::kUseRemote;
}

void DataTypeSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  // Nothing to do if this fails, so just ignore the error it might return.
  ApplyIncrementalSyncChanges(std::move(delete_metadata_change_list),
                              EntityChangeList());
}

void DataTypeSyncBridge::OnCommitAttemptErrors(
    const syncer::FailedCommitResponseDataList& error_response_list) {
  // By default the bridge just ignores failed commit items.
}

void DataTypeSyncBridge::OnSyncPaused() {}

DataTypeSyncBridge::CommitAttemptFailedBehavior
DataTypeSyncBridge::OnCommitAttemptFailed(SyncCommitError commit_error) {
  return CommitAttemptFailedBehavior::kDontRetryOnNextCycle;
}

size_t DataTypeSyncBridge::EstimateSyncOverheadMemoryUsage() const {
  return 0U;
}

sync_pb::EntitySpecifics
DataTypeSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  // Clears all fields by default to avoid the memory and I/O overhead of an
  // additional copy of the data.
  return sync_pb::EntitySpecifics();
}

bool DataTypeSyncBridge::IsEntityDataValid(
    const EntityData& entity_data) const {
  return true;
}

DataTypeLocalChangeProcessor* DataTypeSyncBridge::change_processor() {
  return change_processor_.get();
}

const DataTypeLocalChangeProcessor* DataTypeSyncBridge::change_processor()
    const {
  return change_processor_.get();
}

}  // namespace syncer
