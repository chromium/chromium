// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_FORWARDING_DATA_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_ENGINE_FORWARDING_DATA_TYPE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/sync/engine/data_type_processor.h"

namespace syncer {

// Trivial implementation of DataTypeProcessor, that simply forwards
// call to another processor. This is useful when an API requires transferring
// ownership, but the calling site also wants to keep ownership of the actual
// implementation, and can guarantee the lifetime constraints.
class ForwardingDataTypeProcessor : public DataTypeProcessor {
 public:
  // |processor| must not be null and must outlive this object.
  explicit ForwardingDataTypeProcessor(DataTypeProcessor* processor);

  ForwardingDataTypeProcessor(const ForwardingDataTypeProcessor&) = delete;
  ForwardingDataTypeProcessor& operator=(const ForwardingDataTypeProcessor&) =
      delete;

  ~ForwardingDataTypeProcessor() override;

  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(
      const sync_pb::DataTypeState& type_state,
      const CommitResponseDataList& committed_response_list,
      const FailedCommitResponseDataList& error_response_list) override;
  void OnCommitFailed(SyncCommitError commit_error) override;
  void OnUpdateReceived(
      const sync_pb::DataTypeState& type_state,
      UpdateResponseDataList updates,
      std::optional<sync_pb::GarbageCollectionDirective> gc_directive) override;
  void StorePendingInvalidations(
      std::vector<sync_pb::DataTypeState_Invalidation> invalidations_to_store)
      override;

 private:
  const raw_ptr<DataTypeProcessor> processor_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_FORWARDING_DATA_TYPE_PROCESSOR_H_
