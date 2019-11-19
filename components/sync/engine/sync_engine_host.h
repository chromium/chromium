// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_HOST_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_HOST_H_

#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/protocol/sync_protocol_error.h"

namespace syncer {

class DataTypeDebugInfoListener;
class JsBackend;
class ProtocolEvent;
struct CommitCounters;
struct StatusCounters;
struct UpdateCounters;

// SyncEngineHost is the interface used by SyncEngine to communicate with the
// entity that created it. It's essentially an observer interface except the
// SyncEngine always has exactly one.
class SyncEngineHost {
 public:
  SyncEngineHost();
  virtual ~SyncEngineHost();

  // The engine has completed initialization and it is now ready to accept and
  // process changes. If success is false, initialization wasn't able to be
  // completed and should be retried.
  //
  // |js_backend| is what about:sync interacts with. It is initialized only if
  // |success| is true.
  // TODO(crbug.com/1012226): Remove |last_keystore_key| when VAPID migration is
  // over.
  virtual void OnEngineInitialized(
      ModelTypeSet initial_types,
      const WeakHandle<JsBackend>& js_backend,
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      const std::string& cache_guid,
      const std::string& birthday,
      const std::string& bag_of_chips,
      const std::string& last_keystore_key,
      bool success) = 0;

  // The engine queried the server recently and received some updates.
  virtual void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot,
                                    const std::string& last_keystore_key) = 0;

  // Informs the host of some network event. These notifications are disabled by
  // default and must be enabled through an explicit request to the SyncEngine.
  //
  // It's disabled by default to avoid copying data across threads when no one
  // is listening for it.
  virtual void OnProtocolEvent(const ProtocolEvent& event) = 0;

  // Called when we receive an updated commit counter for a directory type.
  //
  // Disabled by default.  Enable by calling
  // EnableDirectoryTypeDebugInfoForwarding() on the engine.
  virtual void OnDirectoryTypeCommitCounterUpdated(
      ModelType type,
      const CommitCounters& counters) = 0;

  // Called when we receive an updated update counter for a directory type.
  //
  // Disabled by default.  Enable by calling
  // EnableDirectoryTypeDebugInfoForwarding() on the engine.
  virtual void OnDirectoryTypeUpdateCounterUpdated(
      ModelType type,
      const UpdateCounters& counters) = 0;

  // Called when we receive an updated status counter for a datatype.
  //
  // Disabled by default.  Enable by calling
  // EnableDirectoryTypeDebugInfoForwarding() on the engine.
  virtual void OnDatatypeStatusCounterUpdated(
      ModelType type,
      const StatusCounters& counters) = 0;

  // The status of the connection to the sync server has changed.
  virtual void OnConnectionStatusChange(ConnectionStatus status) = 0;

  // Called to perform migration of |types|.
  virtual void OnMigrationNeededForTypes(ModelTypeSet types) = 0;

  // Called when the sync cycle returns there is an user actionable error.
  virtual void OnActionableError(const SyncProtocolError& error) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_HOST_H_
