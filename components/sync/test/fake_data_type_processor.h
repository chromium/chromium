// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "components/sync/engine/data_type_processor.h"

namespace syncer {

class FakeDataTypeProcessor : public DataTypeProcessor {
 public:
  FakeDataTypeProcessor();
  ~FakeDataTypeProcessor() override;

  // DataTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(
      const sync_pb::DataTypeState& type_state,
      const CommitResponseDataList& committed_response_list,
      const FailedCommitResponseDataList& error_response_list) override;
  void OnUpdateReceived(
      const sync_pb::DataTypeState& type_state,
      UpdateResponseDataList updates,
      std::optional<sync_pb::GarbageCollectionDirective> gc_directive) override;
  void StorePendingInvalidations(
      std::vector<sync_pb::DataTypeState_Invalidation> invalidations_to_store)
      override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_DATA_TYPE_PROCESSOR_H_
