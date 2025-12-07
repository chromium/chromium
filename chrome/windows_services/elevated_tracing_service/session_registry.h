// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_SESSION_REGISTRY_H_
#define CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_SESSION_REGISTRY_H_

#include <unknwn.h>

#include <atomic>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "chrome/windows_services/elevated_tracing_service/process_watcher.h"

namespace elevated_tracing_service {

// A registry of active tracing sessions that allows only a single instance to
// be active at a time and monitors that session's client for termination.
class SessionRegistry : public base::RefCountedThreadSafe<SessionRegistry> {
 public:
  class ScopedSession {
   public:
    ScopedSession(const ScopedSession&) = delete;
    ScopedSession& operator=(const ScopedSession&) = delete;
    ~ScopedSession();

   private:
    friend class SessionRegistry;
    ScopedSession(base::Process client_process,
                  base::OnceClosure on_session_destroyed,
                  base::OnceClosure on_client_terminated);

    ProcessWatcher client_process_watcher_;
    base::OnceClosure on_session_destroyed_;
  };

  // Constructs the process-global registry.
  SessionRegistry();
  SessionRegistry(const SessionRegistry&) = delete;
  SessionRegistry& operator=(const SessionRegistry&) = delete;

  // Registers `session`, being used by `client_process`, as the single active
  // session. Returns a scoper to be destroyed along with the session, or null
  // if there already is an active session. This is called on an arbitrary RPC
  // thread.
  static std::unique_ptr<ScopedSession> RegisterActiveSession(
      IUnknown* session,
      base::Process client_process);

  // Sets a closure to be run (on an arbitrary thread) when the active session
  // is cleared.
  void SetSessionClearedClosureForTesting(base::OnceClosure on_session_cleared);

 private:
  class SessionCore;
  friend class ScopedSession;
  friend class base::RefCountedThreadSafe<SessionRegistry>;

  ~SessionRegistry();

  // Called upon destruction of a session (on an arbitrary RPC thread).
  void OnSessionDestroyed(scoped_refptr<SessionCore> core);

  // Called after termination of the client process associated with a session
  // (on an arbitrary thread pool thread).
  void OnClientTerminated(scoped_refptr<SessionCore> core);

  // Clears the active session following either its destruction or termination
  // of the client.
  void ClearActiveSession(SessionCore* core);

  std::atomic<SessionCore*> active_session_ = nullptr;

  // An optional closure to be run (on an arbitrary thread) when the active
  // session is cleared.
  base::OnceClosure on_session_cleared_;
};

}  // namespace elevated_tracing_service

#endif  // CHROME_WINDOWS_SERVICES_ELEVATED_TRACING_SERVICE_SESSION_REGISTRY_H_
