// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/elevated_tracing_service/session_registry.h"

#include <objbase.h>

#include <utility>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"

namespace elevated_tracing_service {

namespace {

SessionRegistry* g_instance = nullptr;

}  // namespace

// SessionRegistry::SessionCore ------------------------------------------------

// A thread-safe holder of a session's primary `IUnknown` pointer.
class SessionRegistry::SessionCore
    : public base::RefCountedThreadSafe<SessionCore> {
 public:
  explicit SessionCore(IUnknown* unknown) : unknown_(unknown) {}

  // Returns the value held, or nullptr if another thread has already taken
  // it.
  IUnknown* release() { return unknown_.exchange(nullptr); }

 private:
  friend class base::RefCountedThreadSafe<SessionCore>;
  ~SessionCore() = default;

  std::atomic<IUnknown*> unknown_;
};

// SessionRegistry::ScopedSession ----------------------------------------------

SessionRegistry::ScopedSession::~ScopedSession() {
  std::move(on_session_destroyed_).Run();
}

SessionRegistry::ScopedSession::ScopedSession(
    base::Process client_process,
    base::OnceClosure on_session_destroyed,
    base::OnceClosure on_client_terminated)
    : client_process_watcher_(std::move(client_process),
                              std::move(on_client_terminated)),
      on_session_destroyed_(std::move(on_session_destroyed)) {}

// SessionRegistry --------------------------------------------

SessionRegistry::SessionRegistry() {
  CHECK_EQ(std::exchange(g_instance, this), nullptr);
}

SessionRegistry::~SessionRegistry() {
  CHECK_EQ(std::exchange(g_instance, nullptr), this);
}

// static
std::unique_ptr<SessionRegistry::ScopedSession>
SessionRegistry::RegisterActiveSession(IUnknown* session,
                                       base::Process client_process) {
  SessionRegistry& instance = CHECK_DEREF(g_instance);
  // Create a new Core instance for this session and make it the current one if
  // there isn't already an active session. Wrap and return the core in a new
  // ScopedSession if it becomes the active session. Otherwise, return null.
  auto core = base::MakeRefCounted<SessionCore>(session);
  SessionCore* expected_null = nullptr;
  return instance.active_session_.compare_exchange_strong(expected_null,
                                                          core.get())
             ? base::WrapUnique(new ScopedSession(
                   std::move(client_process),
                   base::BindOnce(&SessionRegistry::OnSessionDestroyed,
                                  &instance, core),
                   base::BindOnce(&SessionRegistry::OnClientTerminated,
                                  &instance, core)))
             : nullptr;
}

bool SessionRegistry::HasActiveSessionForTesting() const {
  return active_session_.load();
}

void SessionRegistry::OnSessionDestroyed(scoped_refptr<SessionCore> core) {
  // The session is being destroyed cleanly. Clear the IUnknown pointer held in
  // the core so that a race with the client process watcher doesn't try to use
  // it after it has become a dangling pointer.
  if (core->release() != nullptr) {
    // This task is handling session destruction before the termination task, so
    // take responsibility of clearing the active session. From this point
    // onward, a new call to RegisterActiveSession() will succeed.
    SessionCore* expected_core = core.get();
    active_session_.compare_exchange_strong(expected_core, nullptr);
  }
}

void SessionRegistry::OnClientTerminated(scoped_refptr<SessionCore> core) {
  // The client process associated with the session has terminated. If the core
  // still holds the session's IUnknown pointer (meaning that the ScopedSession
  // has yet to be destroyed), tell COM to force a disconnect.
  if (IUnknown* unknown = core->release(); unknown != nullptr) {
    ::CoDisconnectObject(unknown, /*dwReserved=*/0);

    // This task is handling client termination before the session is destroyed,
    // so take responsibility of clearing the active session. From this point
    // onward, a new call to RegisterActiveSession() will succeed.
    SessionCore* expected_core = core.get();
    active_session_.compare_exchange_strong(expected_core, nullptr);
  }
}

}  // namespace elevated_tracing_service
