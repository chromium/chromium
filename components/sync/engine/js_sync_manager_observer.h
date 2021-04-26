// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_JS_SYNC_MANAGER_OBSERVER_H_
#define COMPONENTS_SYNC_ENGINE_JS_SYNC_MANAGER_OBSERVER_H_

#include <string>

#include "base/macros.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/protocol/sync_protocol_error.h"

namespace base {
class Location;
}  // namespace base

namespace syncer {

class JsEventDetails;
class JsEventHandler;

// Routes SyncManager events to a JsEventHandler.
class JsSyncManagerObserver : public SyncManager::Observer {
 public:
  JsSyncManagerObserver();
  ~JsSyncManagerObserver() override;

  void SetJsEventHandler(const WeakHandle<JsEventHandler>& event_handler);

  void InitializationComplete();

  // SyncManager::Observer implementation.
  void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) override;
  void OnConnectionStatusChange(ConnectionStatus status) override;
  void OnActionableError(const SyncProtocolError& sync_protocol_error) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;
  void OnMigrationRequested(ModelTypeSet types) override;

 private:
  void HandleJsEvent(const base::Location& from_here,
                     const std::string& name,
                     const JsEventDetails& details);

  WeakHandle<JsEventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(JsSyncManagerObserver);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_JS_SYNC_MANAGER_OBSERVER_H_
