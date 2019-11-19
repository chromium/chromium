// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/fake_model_type_processor.h"

#include <utility>

#include "base/callback.h"
#include "components/sync/engine/commit_queue.h"

namespace syncer {

FakeModelTypeProcessor::FakeModelTypeProcessor() {}
FakeModelTypeProcessor::~FakeModelTypeProcessor() {}

void FakeModelTypeProcessor::ConnectSync(std::unique_ptr<CommitQueue> worker) {}

void FakeModelTypeProcessor::DisconnectSync() {}

void FakeModelTypeProcessor::GetLocalChanges(size_t max_entries,
                                             GetLocalChangesCallback callback) {
  std::move(callback).Run(CommitRequestDataList());
}
void FakeModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const CommitResponseDataList& response_list) {}

void FakeModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& type_state,
    UpdateResponseDataList updates) {}

}  // namespace syncer
