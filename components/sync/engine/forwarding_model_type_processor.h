// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_FORWARDING_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_ENGINE_FORWARDING_MODEL_TYPE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/sync/engine/model_type_processor.h"

namespace syncer {

// Trivial implementation of ModelTypeProcessor, that simply forwards
// call to another processor. This is useful when an API requires transferring
// ownership, but the calling site also wants to keep ownership of the actual
// implementation, and can guarantee the lifetime constraints.
class ForwardingModelTypeProcessor : public ModelTypeProcessor {
 public:
  // |processor| must not be null and must outlive this object.
  explicit ForwardingModelTypeProcessor(ModelTypeProcessor* processor);

  ForwardingModelTypeProcessor(const ForwardingModelTypeProcessor&) = delete;
  ForwardingModelTypeProcessor& operator=(const ForwardingModelTypeProcessor&) =
      delete;

  ~ForwardingModelTypeProcessor() override;

  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(
      const sync_pb::ModelTypeState& type_state,
      const CommitResponseDataList& committed_response_list,
      const FailedCommitResponseDataList& error_response_list) override;
  void OnCommitFailed(SyncCommitError commit_error) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        UpdateResponseDataList updates,
                        absl::optional<sync_pb::GarbageCollectionDirective>
                            gc_directive) override;
  void StorePendingInvalidations(
      std::vector<sync_pb::ModelTypeState::Invalidation> invalidations_to_store)
      override;

 private:
  const raw_ptr<ModelTypeProcessor> processor_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_FORWARDING_MODEL_TYPE_PROCESSOR_H_
