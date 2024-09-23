// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/mock_update_handler.h"

#include <memory>
#include <string>
#include <utility>

namespace syncer {

MockUpdateHandler::MockUpdateHandler(DataType type) {
  progress_marker_.set_data_type_id(GetSpecificsFieldNumberFromDataType(type));
  const std::string& token_str =
      std::string("Mock token: ") + std::string(DataTypeToDebugString(type));
  progress_marker_.set_token(token_str);
}

MockUpdateHandler::~MockUpdateHandler() = default;

bool MockUpdateHandler::IsInitialSyncEnded() const {
  return false;
}

const sync_pb::DataTypeProgressMarker& MockUpdateHandler::GetDownloadProgress()
    const {
  return progress_marker_;
}

const sync_pb::DataTypeContext& MockUpdateHandler::GetDataTypeContext() const {
  return kEmptyDataTypeContext;
}

void MockUpdateHandler::ProcessGetUpdatesResponse(
    const sync_pb::DataTypeProgressMarker& progress_marker,
    const sync_pb::DataTypeContext& mutated_context,
    const SyncEntityList& applicable_updates,
    StatusController* status) {
  progress_marker_.CopyFrom(progress_marker);
}

void MockUpdateHandler::ApplyUpdates(StatusController* status,
                                     bool cycle_done) {
  apply_updates_count_++;
}

void MockUpdateHandler::RecordRemoteInvalidation(
    std::unique_ptr<SyncInvalidation> incoming) {}

void MockUpdateHandler::CollectPendingInvalidations(
    sync_pb::GetUpdateTriggers* msg) {
  prepare_get_updates_count_++;
}

bool MockUpdateHandler::HasPendingInvalidations() const {
  return false;
}

int MockUpdateHandler::GetApplyUpdatesCount() {
  return apply_updates_count_;
}

int MockUpdateHandler::GetPrepareGetUpdatesCount() {
  return prepare_get_updates_count_;
}

}  // namespace syncer
