// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/data_type_processor_proxy.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/protocol/data_type_state.pb.h"

namespace syncer {

DataTypeProcessorProxy::DataTypeProcessorProxy(
    const base::WeakPtr<DataTypeProcessor>& processor,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : processor_(processor), task_runner_(task_runner) {}

DataTypeProcessorProxy::~DataTypeProcessorProxy() = default;

void DataTypeProcessorProxy::ConnectSync(std::unique_ptr<CommitQueue> worker) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataTypeProcessor::ConnectSync, processor_,
                                std::move(worker)));
}

void DataTypeProcessorProxy::DisconnectSync() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataTypeProcessor::DisconnectSync, processor_));
}

void ForwardGetLocalChangesCall(
    base::WeakPtr<DataTypeProcessor> processor,
    size_t max_entries,
    DataTypeProcessor::GetLocalChangesCallback callback) {
  if (processor) {
    processor->GetLocalChanges(max_entries, std::move(callback));
  } else {
    // If the processor is not valid anymore call the callback to unblock sync
    // thread.
    std::move(callback).Run(CommitRequestDataList());
  }
}

void DataTypeProcessorProxy::GetLocalChanges(size_t max_entries,
                                             GetLocalChangesCallback callback) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ForwardGetLocalChangesCall, processor_,
                                        max_entries, std::move(callback)));
}

void DataTypeProcessorProxy::OnCommitCompleted(
    const sync_pb::DataTypeState& type_state,
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataTypeProcessor::OnCommitCompleted, processor_,
                     type_state, committed_response_list, error_response_list));
}

void DataTypeProcessorProxy::OnCommitFailed(SyncCommitError commit_error) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataTypeProcessor::OnCommitFailed, processor_,
                                commit_error));
}

void DataTypeProcessorProxy::OnUpdateReceived(
    const sync_pb::DataTypeState& type_state,
    UpdateResponseDataList updates,
    std::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataTypeProcessor::OnUpdateReceived, processor_,
                     type_state, std::move(updates), std::move(gc_directive)));
}

void DataTypeProcessorProxy::StorePendingInvalidations(
    std::vector<sync_pb::DataTypeState::Invalidation> invalidations_to_store) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataTypeProcessor::StorePendingInvalidations,
                                processor_, std::move(invalidations_to_store)));
}

}  // namespace syncer
