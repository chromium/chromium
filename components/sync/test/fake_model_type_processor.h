// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_TEST_FAKE_MODEL_TYPE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "components/sync/engine/model_type_processor.h"

namespace syncer {

class FakeModelTypeProcessor : public ModelTypeProcessor {
 public:
  FakeModelTypeProcessor();
  ~FakeModelTypeProcessor() override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(
      const sync_pb::ModelTypeState& type_state,
      const CommitResponseDataList& committed_response_list,
      const FailedCommitResponseDataList& error_response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        UpdateResponseDataList updates,
                        absl::optional<sync_pb::GarbageCollectionDirective>
                            gc_directive) override;
  void StorePendingInvalidations(
      std::vector<sync_pb::ModelTypeState::Invalidation> invalidations_to_store)
      override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_MODEL_TYPE_PROCESSOR_H_
