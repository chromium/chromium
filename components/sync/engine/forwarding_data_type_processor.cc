// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/forwarding_data_type_processor.h"

#include <utility>

#include "base/functional/callback.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/protocol/data_type_state.pb.h"

namespace syncer {

ForwardingDataTypeProcessor::ForwardingDataTypeProcessor(
    DataTypeProcessor* processor)
    : processor_(processor) {
  DCHECK(processor_);
}

ForwardingDataTypeProcessor::~ForwardingDataTypeProcessor() = default;

void ForwardingDataTypeProcessor::ConnectSync(
    std::unique_ptr<CommitQueue> worker) {
  processor_->ConnectSync(std::move(worker));
}

void ForwardingDataTypeProcessor::DisconnectSync() {
  processor_->DisconnectSync();
}

void ForwardingDataTypeProcessor::GetLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  processor_->GetLocalChanges(max_entries, std::move(callback));
}

void ForwardingDataTypeProcessor::OnCommitCompleted(
    const sync_pb::DataTypeState& type_state,
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {
  processor_->OnCommitCompleted(type_state, committed_response_list,
                                error_response_list);
}

void ForwardingDataTypeProcessor::OnCommitFailed(SyncCommitError commit_error) {
  processor_->OnCommitFailed(commit_error);
}

void ForwardingDataTypeProcessor::OnUpdateReceived(
    const sync_pb::DataTypeState& type_state,
    UpdateResponseDataList updates,
    std::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  processor_->OnUpdateReceived(type_state, std::move(updates),
                               std::move(gc_directive));
}

void ForwardingDataTypeProcessor::StorePendingInvalidations(
    std::vector<sync_pb::DataTypeState::Invalidation> invalidations_to_store) {
  processor_->StorePendingInvalidations(std::move(invalidations_to_store));
}

}  // namespace syncer
