// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/model_type_processor_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "components/sync/engine/commit_queue.h"

namespace syncer {

ModelTypeProcessorProxy::ModelTypeProcessorProxy(
    const base::WeakPtr<ModelTypeProcessor>& processor,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : processor_(processor), task_runner_(task_runner) {}

ModelTypeProcessorProxy::~ModelTypeProcessorProxy() {}

void ModelTypeProcessorProxy::ConnectSync(std::unique_ptr<CommitQueue> worker) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ModelTypeProcessor::ConnectSync, processor_,
                                std::move(worker)));
}

void ModelTypeProcessorProxy::DisconnectSync() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ModelTypeProcessor::DisconnectSync, processor_));
}

void ForwardGetLocalChangesCall(
    base::WeakPtr<ModelTypeProcessor> processor,
    size_t max_entries,
    ModelTypeProcessor::GetLocalChangesCallback callback) {
  if (processor) {
    processor->GetLocalChanges(max_entries, std::move(callback));
  } else {
    // If the processor is not valid anymore call the callback to unblock sync
    // thread.
    std::move(callback).Run(CommitRequestDataList());
  }
}

void ModelTypeProcessorProxy::GetLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ForwardGetLocalChangesCall, processor_,
                                        max_entries, std::move(callback)));
}

void ModelTypeProcessorProxy::OnCommitCompleted(
    const sync_pb::ModelTypeState& type_state,
    const CommitResponseDataList& response_list) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ModelTypeProcessor::OnCommitCompleted,
                                        processor_, type_state, response_list));
}

void ModelTypeProcessorProxy::OnUpdateReceived(
    const sync_pb::ModelTypeState& type_state,
    UpdateResponseDataList updates) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ModelTypeProcessor::OnUpdateReceived,
                                processor_, type_state, std::move(updates)));
}

}  // namespace syncer
