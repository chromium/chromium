// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_LOCAL_SESSION_EVENT_ROUTER_H_
#define COMPONENTS_SYNC_SESSIONS_LOCAL_SESSION_EVENT_ROUTER_H_

#include "url/gurl.h"

namespace sync_sessions {

class SyncedTabDelegate;

// An interface defining the ways in which local open tab events can interact
// with session sync.  All local tab events flow to sync via this interface.
// In that way it is analogous to sync changes flowing to the local model
// via ProcessSyncChanges, just with a more granular breakdown.
class LocalSessionEventHandler {
 public:
  LocalSessionEventHandler(const LocalSessionEventHandler&) = delete;
  LocalSessionEventHandler& operator=(const LocalSessionEventHandler&) = delete;

  virtual ~LocalSessionEventHandler() = default;

  // Called when asynchronous session restore has completed. On Android, this
  // can be called multiple times (e.g. transition from a CCT without tabbed
  // window to actually starting a tabbed activity).
  virtual void OnSessionRestoreComplete() = 0;

  // A local navigation event took place that affects the synced session
  // for this instance of Chrome.
  virtual void OnLocalTabModified(SyncedTabDelegate* modified_tab) = 0;

 protected:
  LocalSessionEventHandler() = default;
};

// The LocalSessionEventRouter is responsible for hooking itself up to various
// notification sources in the browser process and forwarding relevant
// events to a handler as defined in the LocalSessionEventHandler contract.
class LocalSessionEventRouter {
 public:
  LocalSessionEventRouter(const LocalSessionEventRouter&) = delete;
  LocalSessionEventRouter& operator=(const LocalSessionEventRouter&) = delete;

  virtual ~LocalSessionEventRouter() = default;
  virtual void StartRoutingTo(LocalSessionEventHandler* handler) = 0;
  virtual void Stop() = 0;

 protected:
  LocalSessionEventRouter() = default;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_LOCAL_SESSION_EVENT_ROUTER_H_
