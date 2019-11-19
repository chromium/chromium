// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_FAKE_SYNCABLE_SERVICE_H_
#define COMPONENTS_SYNC_MODEL_FAKE_SYNCABLE_SERVICE_H_

#include <memory>

#include "components/sync/model/syncable_service.h"

namespace syncer {

class SyncErrorFactory;

// A fake SyncableService that can return arbitrary values and maintains the
// syncing status.
class FakeSyncableService : public SyncableService {
 public:
  FakeSyncableService();
  ~FakeSyncableService() override;

  // Setters for SyncableService implementation results.
  void set_merge_data_and_start_syncing_error(const SyncError& error);
  void set_process_sync_changes_error(const SyncError& error);

  // Whether we're syncing or not. Set on a successful MergeDataAndStartSyncing,
  // unset on StopSyncing. False by default.
  bool syncing() const;

  // SyncableService implementation.
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  SyncMergeResult MergeDataAndStartSyncing(
      ModelType type,
      const SyncDataList& initial_sync_data,
      std::unique_ptr<SyncChangeProcessor> sync_processor,
      std::unique_ptr<SyncErrorFactory> sync_error_factory) override;
  void StopSyncing(ModelType type) override;
  SyncDataList GetAllSyncData(ModelType type) const override;
  SyncError ProcessSyncChanges(const base::Location& from_here,
                               const SyncChangeList& change_list) override;

 private:
  std::unique_ptr<SyncChangeProcessor> sync_processor_;
  SyncError merge_data_and_start_syncing_error_;
  SyncError process_sync_changes_error_;
  bool syncing_;
  ModelType type_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_FAKE_SYNCABLE_SERVICE_H_
