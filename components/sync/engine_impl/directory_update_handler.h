// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_DIRECTORY_UPDATE_HANDLER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_DIRECTORY_UPDATE_HANDLER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/syncer_error.h"
#include "components/sync/engine_impl/process_updates_util.h"
#include "components/sync/engine_impl/update_handler.h"

namespace sync_pb {
class DataTypeProgressMarker;
}

namespace syncer {

class DataTypeDebugInfoEmitter;
class ModelSafeWorker;
class StatusController;

namespace syncable {
class Directory;
}

// This class represents the syncable::Directory's processes for requesting and
// processing updates from the sync server.
//
// Each instance of this class represents a particular type in the
// syncable::Directory.  It can store and retreive that type's progress markers.
// It can also process a set of received SyncEntities and store their data.
class DirectoryUpdateHandler : public UpdateHandler {
 public:
  DirectoryUpdateHandler(syncable::Directory* dir,
                         ModelType type,
                         scoped_refptr<ModelSafeWorker> worker,
                         DataTypeDebugInfoEmitter* debug_info_emitter);
  ~DirectoryUpdateHandler() override;

  // UpdateHandler implementation.
  bool IsInitialSyncEnded() const override;
  void GetDownloadProgress(
      sync_pb::DataTypeProgressMarker* progress_marker) const override;
  void GetDataTypeContext(sync_pb::DataTypeContext* context) const override;
  SyncerError ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const sync_pb::DataTypeContext& mutated_context,
      const SyncEntityList& applicable_updates,
      StatusController* status) override;
  void ApplyUpdates(StatusController* status) override;
  void PassiveApplyUpdates(StatusController* status) override;

 private:
  friend class DirectoryUpdateHandlerApplyUpdateTest;
  friend class DirectoryUpdateHandlerProcessUpdateTest;

  // Sometimes there is nothing to do, so we can return without doing anything.
  bool IsApplyUpdatesRequired();

  // Called at the end of ApplyUpdates and PassiveApplyUpdates and performs
  // steps common to both (even when IsApplyUpdatesRequired has returned
  // false).
  void PostApplyUpdates();

  // Processes the given SyncEntities and stores their data in the directory.
  // Their types must match this update handler's type.
  void UpdateSyncEntities(syncable::ModelNeutralWriteTransaction* trans,
                          const SyncEntityList& applicable_updates,
                          bool is_initial_sync,
                          StatusController* status);

  // Expires entries according to GC directives.
  void ExpireEntriesIfNeeded(
      syncable::ModelNeutralWriteTransaction* trans,
      const sync_pb::DataTypeProgressMarker& progress_marker);

  // Stores the given progress marker in the directory.
  // Its type must match this update handler's type.
  void UpdateProgressMarker(
      const sync_pb::DataTypeProgressMarker& progress_marker);

  bool IsValidProgressMarker(
      const sync_pb::DataTypeProgressMarker& progress_marker) const;

  // Skips all checks and goes straight to applying the updates.
  SyncerError ApplyUpdatesImpl(StatusController* status);

  // Creates root node for the handled model type.
  void CreateTypeRoot(syncable::ModelNeutralWriteTransaction* trans);

  syncable::Directory* dir_;
  ModelType type_;
  scoped_refptr<ModelSafeWorker> worker_;
  DataTypeDebugInfoEmitter* debug_info_emitter_;

  // The version which directory already ran garbage collection against on.
  int64_t cached_gc_directive_version_;

  // The day which directory already ran garbage collection against on.
  base::Time cached_gc_directive_aged_out_day_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryUpdateHandler);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_DIRECTORY_UPDATE_HANDLER_H_
