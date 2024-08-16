// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_HOST_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_HOST_H_

#include "components/sync/base/data_type.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/engine/sync_protocol_error.h"

namespace syncer {

class ProtocolEvent;

// SyncEngineHost is the interface used by SyncEngine to communicate with the
// entity that created it. It's essentially an observer interface except the
// SyncEngine always has exactly one.
class SyncEngineHost {
 public:
  SyncEngineHost() = default;
  virtual ~SyncEngineHost() = default;

  // The engine has completed initialization and it is now ready to accept and
  // process changes. If success is false, initialization wasn't able to be
  // completed and should be retried.
  //
  // |js_backend| is what chrome://sync-internals interacts with. It is
  // initialized only if |success| is true.

  virtual void OnEngineInitialized(bool success,
                                   bool is_first_time_sync_configure) = 0;

  // The engine queried the server recently and received some updates.
  virtual void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) = 0;

  // Informs the host of some network event. These notifications are disabled by
  // default and must be enabled through an explicit request to the SyncEngine.
  //
  // It's disabled by default to avoid copying data across threads when no one
  // is listening for it.
  virtual void OnProtocolEvent(const ProtocolEvent& event) = 0;

  // The status of the connection to the sync server has changed.
  virtual void OnConnectionStatusChange(ConnectionStatus status) = 0;

  // Called to perform migration of |types|.
  virtual void OnMigrationNeededForTypes(DataTypeSet types) = 0;

  // Called when the sync cycle returns there is an user actionable error.
  virtual void OnActionableProtocolError(const SyncProtocolError& error) = 0;

  // Called when the set of backed off types is changed.
  virtual void OnBackedOffTypesChanged() = 0;

  // Called when invalidations are enabled or disabled.
  virtual void OnInvalidationStatusChanged() = 0;

  // Called when there are new data types with pending invalidations.
  virtual void OnNewInvalidatedDataTypes() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_HOST_H_
