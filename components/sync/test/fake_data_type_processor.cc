// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_data_type_processor.h"

#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/protocol/data_type_state.pb.h"

namespace syncer {

FakeDataTypeProcessor::FakeDataTypeProcessor() = default;
FakeDataTypeProcessor::~FakeDataTypeProcessor() = default;

void FakeDataTypeProcessor::ConnectSync(std::unique_ptr<CommitQueue> worker) {}

void FakeDataTypeProcessor::DisconnectSync() {}

void FakeDataTypeProcessor::GetLocalChanges(size_t max_entries,
                                            GetLocalChangesCallback callback) {
  std::move(callback).Run(CommitRequestDataList());
}
void FakeDataTypeProcessor::OnCommitCompleted(
    const sync_pb::DataTypeState& type_state,
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {}

void FakeDataTypeProcessor::OnUpdateReceived(
    const sync_pb::DataTypeState& type_state,
    UpdateResponseDataList updates,
    std::optional<sync_pb::GarbageCollectionDirective> gc_directive) {}
void FakeDataTypeProcessor::StorePendingInvalidations(
    std::vector<sync_pb::DataTypeState::Invalidation> invalidations_to_store) {}

}  // namespace syncer
