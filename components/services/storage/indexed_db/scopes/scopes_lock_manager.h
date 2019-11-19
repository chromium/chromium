// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_SCOPES_LOCK_MANAGER_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_SCOPES_LOCK_MANAGER_H_

#include <iosfwd>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/services/storage/indexed_db/scopes/scope_lock.h"
#include "components/services/storage/indexed_db/scopes/scope_lock_range.h"

namespace content {

// Used to receive and hold locks from a ScopesLockManager. This struct enables
// the ScopeLock objects to always live in the destination of the caller's
// choosing (as opposed to having the locks be an argument in the callback,
// where they could be owned by the task scheduler).
// This class must be used and destructed on the same sequence as the
// ScopesLockManager.
struct ScopesLocksHolder {
 public:
  ScopesLocksHolder();
  ~ScopesLocksHolder();

  base::WeakPtr<ScopesLocksHolder> AsWeakPtr() {
    return weak_factory.GetWeakPtr();
  }

  void AbortLockRequest() { weak_factory.InvalidateWeakPtrs(); }

  std::vector<ScopeLock> locks;
  base::WeakPtrFactory<ScopesLocksHolder> weak_factory{this};

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopesLocksHolder);
};

// Generic two-level lock management system based on ranges. Granted locks are
// represented by the |ScopeLock| class.
class ScopesLockManager {
 public:
  using LocksAquiredCallback = base::OnceClosure;

  // Shared locks can share access to a lock range, while exclusive locks
  // require that they are the only lock for their range.
  enum class LockType { kShared, kExclusive };

  ScopesLockManager();
  virtual ~ScopesLockManager();

  virtual int64_t LocksHeldForTesting() const = 0;
  virtual int64_t RequestsWaitingForTesting() const = 0;

  // Acquires locks for the given requests. Lock levels are treated as
  // completely independent domains. The lock levels start at zero.
  // Returns false if any of the lock ranges were invalid or an invariant was
  // broken.
  struct ScopeLockRequest {
    ScopeLockRequest(int level, ScopeLockRange range, LockType type);
    int level;
    ScopeLockRange range;
    LockType type;
  };
  virtual bool AcquireLocks(base::flat_set<ScopeLockRequest> lock_requests,
                            base::WeakPtr<ScopesLocksHolder> locks_receiever,
                            LocksAquiredCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopesLockManager);

  base::WeakPtrFactory<ScopesLockManager> weak_factory_{this};
};

bool operator<(const ScopesLockManager::ScopeLockRequest& x,
               const ScopesLockManager::ScopeLockRequest& y);
bool operator==(const ScopesLockManager::ScopeLockRequest& x,
                const ScopesLockManager::ScopeLockRequest& y);
bool operator!=(const ScopesLockManager::ScopeLockRequest& x,
                const ScopesLockManager::ScopeLockRequest& y);

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_SCOPES_LOCK_MANAGER_H_
