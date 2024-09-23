// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_H_
#define COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"

namespace syncer {

class SyncChangeProcessor;

// DEPRECATED: new code should use DataTypeSyncBridge instead.
// See https://www.chromium.org/developers/design-documents/sync/model-api/ for
// background.
class SyncableService {
 public:
  SyncableService() = default;

  SyncableService(const SyncableService&) = delete;
  SyncableService& operator=(const SyncableService&) = delete;

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
  // browser's IO-intensive startup process is over. The DataType parameter is
  // included so that the receiving end can track usage and timing statistics,
  // make optimizations or tradeoffs by type, etc.
  using StartSyncFlare = base::RepeatingCallback<void(DataType)>;

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
  // Returns: std::nullopt if no error was encountered while merging the two
  //          models, otherwise a std::optional filled with such error.
  virtual std::optional<syncer::ModelError> MergeDataAndStartSyncing(
      DataType type,
      const SyncDataList& initial_sync_data,
      std::unique_ptr<SyncChangeProcessor> sync_processor) = 0;

  // Stop syncing the specified type and reset state.
  virtual void StopSyncing(DataType type) = 0;

  // Notifies the syncable service to stop syncing on browser shutdown. This is
  // a separate method from StopSyncing() to let implementations do something
  // different in case of shutdown.
  virtual void OnBrowserShutdown(DataType type);

  // SyncChangeProcessor interface.
  // Process a list of new SyncChanges and update the local data as necessary.
  // Returns: std::nullopt if no error was encountered, otherwise a
  //          std::optional filled with such error.
  virtual std::optional<ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const SyncChangeList& change_list) = 0;

  // Get a WeakPtr to the instance.
  virtual base::WeakPtr<SyncableService> AsWeakPtr() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNCABLE_SERVICE_H_
