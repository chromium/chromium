// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_ENGINE_MOCK_UPDATE_HANDLER_H_
#define COMPONENTS_SYNC_TEST_ENGINE_MOCK_UPDATE_HANDLER_H_

#include "base/compiler_specific.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine_impl/update_handler.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

class MockUpdateHandler : public UpdateHandler {
 public:
  explicit MockUpdateHandler(ModelType type);
  ~MockUpdateHandler() override;

  // UpdateHandler implementation.
  bool IsInitialSyncEnded() const override;
  const sync_pb::DataTypeProgressMarker& GetDownloadProgress() const override;
  const sync_pb::DataTypeContext& GetDataTypeContext() const override;
  SyncerError ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const sync_pb::DataTypeContext& mutated_context,
      const SyncEntityList& applicable_updates,
      StatusController* status) override;
  void ApplyUpdates(StatusController* status) override;
  void PassiveApplyUpdates(StatusController* status) override;

  // Returns the number of times ApplyUpdates() was invoked.
  int GetApplyUpdatesCount();

  // Returns the number of times PassiveApplyUpdates() was invoked.
  int GetPassiveApplyUpdatesCount();

 private:
  sync_pb::DataTypeProgressMarker progress_marker_;
  const sync_pb::DataTypeContext kEmptyDataTypeContext;

  int apply_updates_count_;
  int passive_apply_updates_count_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_ENGINE_MOCK_UPDATE_HANDLER_H_
