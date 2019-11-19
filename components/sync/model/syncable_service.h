// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_H_
#define COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_merge_result.h"

namespace syncer {

class SyncChangeProcessor;
class SyncErrorFactory;

// TODO(zea): remove SupportsWeakPtr in favor of having all SyncableService
// implementers provide a way of getting a weak pointer to themselves.
// See crbug.com/100114.
class SyncableService : public base::SupportsWeakPtr<SyncableService> {
 public:
  SyncableService() = default;
  virtual ~SyncableService() = default;

  // A StartSyncFlare is useful when your SyncableService has a need for sync
  // to start ASAP. This is typically for one of three reasons:
  // 1) Because a local change event has occurred but MergeDataAndStartSyncing
  // hasn't been called yet, meaning you don't have a SyncChangeProcessor. The
  // sync subsystem will respond soon after invoking Run() on your flare by
  // calling MergeDataAndStartSyncing.
  // 2) You want remote data to be visible immediately; for example if the
  // history page is open, you want remote sessions data to be available there.
  // 3) You want to signal to sync that it's safe to start now that the
  // browser's IO-intensive startup process is over. The ModelType parameter is
  // included so that the recieving end can track usage and timing statistics,
  // make pptimizations or tradeoffs by type, etc.
  using StartSyncFlare = base::Callback<void(ModelType)>;

  // Allows the SyncableService to delay sync events (all below) until the model
  // becomes ready to sync. Callers must ensure there is no previous ongoing
  // wait (per datatype, if the SyncableService supports multiple).
  virtual void WaitUntilReadyToSync(base::OnceClosure done) = 0;

  // Informs the service to begin syncing the specified synced datatype |type|.
  // The service should then merge |initial_sync_data| into it's local data,
  // calling |sync_processor|'s ProcessSyncChanges as necessary to reconcile the
  // two. After this, the SyncableService's local data should match the server
  // data, and the service should be ready to receive and process any further
  // SyncChange's as they occur.
  // Returns: a SyncMergeResult whose error field reflects whether an error
  //          was encountered while merging the two models. The merge result
  //          may also contain optional merge statistics.
  virtual SyncMergeResult MergeDataAndStartSyncing(
      ModelType type,
      const SyncDataList& initial_sync_data,
      std::unique_ptr<SyncChangeProcessor> sync_processor,
      std::unique_ptr<SyncErrorFactory> error_handler) = 0;

  // Stop syncing the specified type and reset state.
  virtual void StopSyncing(ModelType type) = 0;

  // SyncChangeProcessor interface.
  // Process a list of new SyncChanges and update the local data as necessary.
  // Returns: A default SyncError (IsSet() == false) if no errors were
  //          encountered, and a filled SyncError (IsSet() == true)
  //          otherwise.
  virtual SyncError ProcessSyncChanges(const base::Location& from_here,
                                       const SyncChangeList& change_list) = 0;

  // TODO(crbug.com/870624): We don't seem to use this function anywhere, so
  // we should simply remove it and simplify all implementations.
  virtual SyncDataList GetAllSyncData(ModelType type) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncableService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_H_
