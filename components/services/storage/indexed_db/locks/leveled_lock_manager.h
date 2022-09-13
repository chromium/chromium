// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_LEVELED_LOCK_MANAGER_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_LEVELED_LOCK_MANAGER_H_

#include <iosfwd>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "components/services/storage/indexed_db/locks/leveled_lock.h"
#include "components/services/storage/indexed_db/locks/leveled_lock_range.h"

namespace content {

// Used to receive and hold locks from a LeveledLockManager. This struct enables
// the LeveledLock objects to always live in the destination of the caller's
// choosing (as opposed to having the locks be an argument in the callback,
// where they could be owned by the task scheduler).
// This class must be used and destructed on the same sequence as the
// LeveledLockManager.
struct COMPONENT_EXPORT(LOCK_MANAGER) LeveledLockHolder {
  LeveledLockHolder();
  LeveledLockHolder(const LeveledLockHolder&) = delete;
  LeveledLockHolder& operator=(const LeveledLockHolder&) = delete;
  ~LeveledLockHolder();

  base::WeakPtr<LeveledLockHolder> AsWeakPtr() {
    return weak_factory.GetWeakPtr();
  }

  void AbortLockRequest() { weak_factory.InvalidateWeakPtrs(); }

  std::vector<LeveledLock> locks;
  base::WeakPtrFactory<LeveledLockHolder> weak_factory{this};
};

// Generic two-level lock management system based on ranges. Granted locks are
// represented by the |LeveledLock| class.
class COMPONENT_EXPORT(LOCK_MANAGER) LeveledLockManager {
 public:
  using LocksAcquiredCallback = base::OnceClosure;

  // Shared locks can share access to a lock range, while exclusive locks
  // require that they are the only lock for their range.
  enum class LockType { kShared, kExclusive };

  LeveledLockManager();
  LeveledLockManager(const LeveledLockManager&) = delete;
  LeveledLockManager& operator=(const LeveledLockManager&) = delete;
  virtual ~LeveledLockManager();

  virtual int64_t LocksHeldForTesting() const = 0;
  virtual int64_t RequestsWaitingForTesting() const = 0;

  // Acquires locks for the given requests. Lock levels are treated as
  // completely independent domains. The lock levels start at zero.
  // Returns false if any of the lock ranges were invalid or an invariant was
  // broken.
  struct COMPONENT_EXPORT(LOCK_MANAGER) LeveledLockRequest {
    LeveledLockRequest(int level, LeveledLockRange range, LockType type);
    int level;
    LeveledLockRange range;
    LockType type;
  };
  virtual bool AcquireLocks(base::flat_set<LeveledLockRequest> lock_requests,
                            base::WeakPtr<LeveledLockHolder> locks_receiever,
                            LocksAcquiredCallback callback) = 0;

 private:
  base::WeakPtrFactory<LeveledLockManager> weak_factory_{this};
};

COMPONENT_EXPORT(LOCK_MANAGER)
bool operator<(const LeveledLockManager::LeveledLockRequest& x,
               const LeveledLockManager::LeveledLockRequest& y);
COMPONENT_EXPORT(LOCK_MANAGER)
bool operator==(const LeveledLockManager::LeveledLockRequest& x,
                const LeveledLockManager::LeveledLockRequest& y);
COMPONENT_EXPORT(LOCK_MANAGER)
bool operator!=(const LeveledLockManager::LeveledLockRequest& x,
                const LeveledLockManager::LeveledLockRequest& y);

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_LEVELED_LOCK_MANAGER_H_
