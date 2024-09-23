// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_UPDATE_HANDLER_H_
#define COMPONENTS_SYNC_TEST_MOCK_UPDATE_HANDLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_invalidation.h"
#include "components/sync/engine/update_handler.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"

namespace syncer {

class MockUpdateHandler : public UpdateHandler {
 public:
  explicit MockUpdateHandler(DataType type);
  ~MockUpdateHandler() override;

  // UpdateHandler implementation.
  bool IsInitialSyncEnded() const override;
  const sync_pb::DataTypeProgressMarker& GetDownloadProgress() const override;
  const sync_pb::DataTypeContext& GetDataTypeContext() const override;
  void ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const sync_pb::DataTypeContext& mutated_context,
      const SyncEntityList& applicable_updates,
      StatusController* status) override;
  void ApplyUpdates(StatusController* status, bool cycle_done) override;
  void RecordRemoteInvalidation(
      std::unique_ptr<SyncInvalidation> incoming) override;
  void CollectPendingInvalidations(sync_pb::GetUpdateTriggers* msg) override;
  bool HasPendingInvalidations() const override;

  // Returns the number of times ApplyUpdates() was invoked.
  int GetApplyUpdatesCount();
  // Returns the number of times PrepareGetUpdates() was invoked.
  int GetPrepareGetUpdatesCount();

 private:
  sync_pb::DataTypeProgressMarker progress_marker_;
  const sync_pb::DataTypeContext kEmptyDataTypeContext;
  int apply_updates_count_ = 0;
  int prepare_get_updates_count_ = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_UPDATE_HANDLER_H_
