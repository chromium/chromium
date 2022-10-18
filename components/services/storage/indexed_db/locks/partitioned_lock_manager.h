// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_MANAGER_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_MANAGER_H_

#include <iosfwd>
#include <list>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"

namespace content {

// Used to receive and hold locks from a PartitionedLockManager. This struct
// enables the PartitionedLock objects to always live in the destination of the
// caller's choosing (as opposed to having the locks be an argument in the
// callback, where they could be owned by the task scheduler).
//
// This class must be used and destructed on the same sequence as the
// PartitionedLockManager.
struct COMPONENT_EXPORT(LOCK_MANAGER) PartitionedLockHolder {
  PartitionedLockHolder();
  PartitionedLockHolder(const PartitionedLockHolder&) = delete;
  PartitionedLockHolder& operator=(const PartitionedLockHolder&) = delete;
  ~PartitionedLockHolder();

  base::WeakPtr<PartitionedLockHolder> AsWeakPtr() {
    return weak_factory.GetWeakPtr();
  }

  void AbortLockRequest() { weak_factory.InvalidateWeakPtrs(); }

  std::vector<PartitionedLock> locks;
  base::WeakPtrFactory<PartitionedLockHolder> weak_factory{this};
};

// Generic two-level lock management system based on lock_ids. Granted locks are
// represented by the |PartitionedLock| class.
class COMPONENT_EXPORT(LOCK_MANAGER) PartitionedLockManager {
 public:
  using LocksAcquiredCallback = base::OnceClosure;

  // Shared locks can share access to a lock id, while exclusive locks
  // require that they are the only lock for their lock id.
  enum class LockType { kShared, kExclusive };

  PartitionedLockManager();
  PartitionedLockManager(const PartitionedLockManager&) = delete;
  PartitionedLockManager& operator=(const PartitionedLockManager&) = delete;
  virtual ~PartitionedLockManager();

  virtual int64_t LocksHeldForTesting() const = 0;
  virtual int64_t RequestsWaitingForTesting() const = 0;

  // Acquires locks for the given requests. Lock partitions are treated as
  // completely independent domains.
  struct COMPONENT_EXPORT(LOCK_MANAGER) PartitionedLockRequest {
    PartitionedLockRequest(PartitionedLockId lock_id, LockType type);
    PartitionedLockId lock_id;
    LockType type;
  };
  virtual void AcquireLocks(
      base::flat_set<PartitionedLockRequest> lock_requests,
      base::WeakPtr<PartitionedLockHolder> locks_receiever,
      LocksAcquiredCallback callback) = 0;

  enum class TestLockResult { kLocked, kFree };
  virtual TestLockResult TestLock(PartitionedLockRequest lock_requests) = 0;

 private:
  base::WeakPtrFactory<PartitionedLockManager> weak_factory_{this};
};

COMPONENT_EXPORT(LOCK_MANAGER)
bool operator<(const PartitionedLockManager::PartitionedLockRequest& x,
               const PartitionedLockManager::PartitionedLockRequest& y);
COMPONENT_EXPORT(LOCK_MANAGER)
bool operator==(const PartitionedLockManager::PartitionedLockRequest& x,
                const PartitionedLockManager::PartitionedLockRequest& y);
COMPONENT_EXPORT(LOCK_MANAGER)
bool operator!=(const PartitionedLockManager::PartitionedLockRequest& x,
                const PartitionedLockManager::PartitionedLockRequest& y);

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_MANAGER_H_
