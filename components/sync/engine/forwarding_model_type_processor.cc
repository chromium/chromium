// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/forwarding_model_type_processor.h"

#include <utility>
#include "base/callback.h"
#include "components/sync/engine/commit_queue.h"

namespace syncer {

ForwardingModelTypeProcessor::ForwardingModelTypeProcessor(
    ModelTypeProcessor* processor)
    : processor_(processor) {
  DCHECK(processor_);
}

ForwardingModelTypeProcessor::~ForwardingModelTypeProcessor() = default;

void ForwardingModelTypeProcessor::ConnectSync(
    std::unique_ptr<CommitQueue> worker) {
  processor_->ConnectSync(std::move(worker));
}

void ForwardingModelTypeProcessor::DisconnectSync() {
  processor_->DisconnectSync();
}

void ForwardingModelTypeProcessor::GetLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  processor_->GetLocalChanges(max_entries, std::move(callback));
}

void ForwardingModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {
  processor_->OnCommitCompleted(type_state, committed_response_list,
                                error_response_list);
}

void ForwardingModelTypeProcessor::OnCommitFailed(
    SyncCommitError commit_error) {
  processor_->OnCommitFailed(commit_error);
}

void ForwardingModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& type_state,
    UpdateResponseDataList updates,
    absl::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  processor_->OnUpdateReceived(type_state, std::move(updates),
                               std::move(gc_directive));
}

void ForwardingModelTypeProcessor::StorePendingInvalidations(
    std::vector<sync_pb::ModelTypeState::Invalidation> invalidations_to_store) {
  processor_->StorePendingInvalidations(std::move(invalidations_to_store));
}

}  // namespace syncer
