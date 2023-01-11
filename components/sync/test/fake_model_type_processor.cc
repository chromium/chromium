// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_model_type_processor.h"

#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "components/sync/engine/commit_queue.h"

namespace syncer {

FakeModelTypeProcessor::FakeModelTypeProcessor() = default;
FakeModelTypeProcessor::~FakeModelTypeProcessor() = default;

void FakeModelTypeProcessor::ConnectSync(std::unique_ptr<CommitQueue> worker) {}

void FakeModelTypeProcessor::DisconnectSync() {}

void FakeModelTypeProcessor::GetLocalChanges(size_t max_entries,
                                             GetLocalChangesCallback callback) {
  std::move(callback).Run(CommitRequestDataList());
}
void FakeModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {}

void FakeModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& type_state,
    UpdateResponseDataList updates,
    absl::optional<sync_pb::GarbageCollectionDirective> gc_directive) {}
void FakeModelTypeProcessor::StorePendingInvalidations(
    std::vector<sync_pb::ModelTypeState::Invalidation> invalidations_to_store) {
}

}  // namespace syncer
