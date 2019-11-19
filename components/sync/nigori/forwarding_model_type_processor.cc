// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/forwarding_model_type_processor.h"

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
    const CommitResponseDataList& response_list) {
  processor_->OnCommitCompleted(type_state, response_list);
}

void ForwardingModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& type_state,
    UpdateResponseDataList updates) {
  processor_->OnUpdateReceived(type_state, std::move(updates));
}

}  // namespace syncer
