// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_ENGINE_MODEL_TYPE_PROCESSOR_H_

#include <memory>

#include "base/callback_forward.h"
#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {
class CommitQueue;

// Interface used by sync backend to issue requests to a synced data type.
class ModelTypeProcessor {
 public:
  ModelTypeProcessor();
  virtual ~ModelTypeProcessor();

  // Connect this processor to the sync engine via |commit_queue|. Once called,
  // the processor will send any pending and future commits via this channel.
  // This can only be called multiple times if the processor is disconnected
  // (via the DataTypeController) in between.
  virtual void ConnectSync(std::unique_ptr<CommitQueue> commit_queue) = 0;

  // Disconnect this processor from the sync engine. Change metadata will
  // continue being processed and persisted, but no commits can be made until
  // the next time sync is connected.
  virtual void DisconnectSync() = 0;

  // Sync engine calls GetLocalChanges to request local entities to be committed
  // to server. Processor should call callback passing local entites when they
  // are ready. Processor should not pass more than |max_entities|.
  using GetLocalChangesCallback =
      base::OnceCallback<void(CommitRequestDataList&&)>;
  virtual void GetLocalChanges(size_t max_entries,
                               GetLocalChangesCallback callback) = 0;

  // Informs this object that some of its commit requests have been
  // successfully serviced.
  virtual void OnCommitCompleted(
      const sync_pb::ModelTypeState& type_state,
      const CommitResponseDataList& response_list) = 0;

  // Informs this object that there are some incoming updates it should
  // handle.
  virtual void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                                UpdateResponseDataList updates) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_MODEL_TYPE_PROCESSOR_H_
