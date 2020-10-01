// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_UPDATE_HANDLER_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_UPDATE_HANDLER_H_

#include <vector>

#include "components/sync/base/syncer_error.h"

namespace sync_pb {
class DataTypeContext;
class DataTypeProgressMarker;
class SyncEntity;
}

using SyncEntityList = std::vector<const sync_pb::SyncEntity*>;

namespace syncer {

class StatusController;

// This class represents an entity that can request, receive, and apply updates
// from the sync server.
class UpdateHandler {
 public:
  virtual ~UpdateHandler() = default;

  // Returns true if initial sync was performed for this type.
  virtual bool IsInitialSyncEnded() const = 0;

  // Returns the stored progress marker for this type.
  virtual const sync_pb::DataTypeProgressMarker& GetDownloadProgress()
      const = 0;

  // Returns the per-client datatype context.
  virtual const sync_pb::DataTypeContext& GetDataTypeContext() const = 0;

  // Processes the contents of a GetUpdates response message.
  //
  // Should be invoked with the progress marker and set of SyncEntities from a
  // single GetUpdates response message.  The progress marker's type must match
  // this update handler's type, and the set of SyncEntities must include all
  // entities of this type found in the response message.
  //
  // In this context, "applicable_updates" means the set of updates belonging to
  // this type.
  //
  // Returns SYNCER_OK if the all data was processed successfully, a syncer
  // error otherwise.
  virtual SyncerError ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const sync_pb::DataTypeContext& mutated_context,
      const SyncEntityList& applicable_updates,
      StatusController* status) = 0;

  // Called at the end of a non-configure GetUpdates loop to apply any unapplied
  // updates.
  virtual void ApplyUpdates(StatusController* status) = 0;

  // Called at the end of a configure GetUpdates loop to perform any required
  // post-initial-download update application.
  virtual void PassiveApplyUpdates(StatusController* status) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_UPDATE_HANDLER_H_
