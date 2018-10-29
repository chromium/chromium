// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_SYNC_CHANGE_PROCESSOR_H_

#include <string>
#include <vector>

#include "components/sync/base/model_type.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error.h"

namespace base {
class Location;
}  // namespace base

namespace syncer {

class LocalChangeObserver;

// An interface for services that handle receiving SyncChanges.
class SyncChangeProcessor {
 public:
  // Whether a context change should force a datatype refresh or not.
  enum ContextRefreshStatus { NO_REFRESH, REFRESH_NEEDED };

  SyncChangeProcessor();
  virtual ~SyncChangeProcessor();

  // Process a list of SyncChanges.
  // Returns: A default SyncError (IsSet() == false) if no errors were
  //          encountered, and a filled SyncError (IsSet() == true)
  //          otherwise.
  // Inputs:
  //   |from_here|: allows tracking of where sync changes originate.
  //   |change_list|: is the list of sync changes in need of processing.
  virtual SyncError ProcessSyncChanges(const base::Location& from_here,
                                       const SyncChangeList& change_list) = 0;

  // Fills a list of SyncData. This should create an up to date representation
  // of all the data known to the ChangeProcessor for |datatype|, and
  // should match/be a subset of the server's view of that datatype.
  //
  // WARNING: This can be a potentially slow & memory intensive operation and
  // should only be used when absolutely necessary / sparingly.
  virtual SyncDataList GetAllSyncData(ModelType type) const = 0;

  // Updates the context for |type|, triggering an optional context refresh.
  // Default implementation does nothing. A type's context is a per-client blob
  // that can affect all SyncData sent to/from the server, much like a cookie.
  // TODO(zea): consider pulling the refresh logic into a separate method
  // unrelated to datatype implementations.
  virtual SyncError UpdateDataTypeContext(ModelType type,
                                          ContextRefreshStatus refresh_status,
                                          const std::string& context);

  // Adds an observer of local sync changes. This observer is notified when
  // local sync changes are applied by GenericChangeProcessor. observer is
  // not owned by the SyncChangeProcessor.
  virtual void AddLocalChangeObserver(LocalChangeObserver* observer);
  virtual void RemoveLocalChangeObserver(LocalChangeObserver* observer);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_CHANGE_PROCESSOR_H_
