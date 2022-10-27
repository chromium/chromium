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
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
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

// Holds locks of the scopes system. Granted locks are represented by the
// |PartitionedLock| class.
//
//  Invariants for the lock management system:
// * All calls must happen from the same sequenced task runner.
// * Locks are granted in the order in which they are requested.
// * Locks held by an entity must be acquired all at once. If more locks are
//   needed (where old locks will continue to be held), then all locks must be
//   released first, and then all necessary locks acquired in one acquisition
//   call.
class COMPONENT_EXPORT(LOCK_MANAGER) PartitionedLockManager {
 public:
  using LocksAcquiredCallback = base::OnceClosure;

  // Shared locks can share access to a lock id, while exclusive locks
  // require that they are the only lock for their lock id.
  enum class LockType { kShared, kExclusive };

  // Grabs the current task runner that we are running on to be used for the
  // lock acquisition callbacks.
  PartitionedLockManager();

  PartitionedLockManager(const PartitionedLockManager&) = delete;
  PartitionedLockManager& operator=(const PartitionedLockManager&) = delete;

  ~PartitionedLockManager();

  int64_t LocksHeldForTesting() const;
  int64_t RequestsWaitingForTesting() const;

  // Acquires locks for the given requests. Lock partitions are treated as
  // completely independent domains.
  struct COMPONENT_EXPORT(LOCK_MANAGER) PartitionedLockRequest {
    PartitionedLockRequest(PartitionedLockId lock_id, LockType type);
    PartitionedLockId lock_id;
    LockType type;
  };
  struct COMPONENT_EXPORT(LOCK_MANAGER) AcquireOptions {
    AcquireOptions();
    bool ensure_async = false;
  };
  void AcquireLocks(base::flat_set<PartitionedLockRequest> lock_requests,
                    base::WeakPtr<PartitionedLockHolder> locks_holder,
                    LocksAcquiredCallback callback,
                    AcquireOptions acquire_options = AcquireOptions());

  enum class TestLockResult { kLocked, kFree };
  // Tests to see if the given lock request can be acquired.
  TestLockResult TestLock(PartitionedLockRequest lock_requests);

  // Remove the given lock lock_id. The lock lock_id must not be in use. Call
  // this if the lock will never be used again.
  void RemoveLockId(const PartitionedLockId& lock_id);

 private:
  struct LockRequest {
   public:
    LockRequest();
    LockRequest(LockRequest&&) noexcept;
    LockRequest(LockType type,
                base::WeakPtr<PartitionedLockHolder> locks_holder,
                base::OnceClosure callback);
    ~LockRequest();

    LockType requested_type = LockType::kShared;
    base::WeakPtr<PartitionedLockHolder> locks_holder;
    base::OnceClosure acquired_callback;
  };

  // Represents a lock, which has a lock_id. To support shared access, there can
  // be multiple acquisitions of this lock, represented in |acquired_count|.
  // Also holds the pending requests for this lock.
  struct Lock {
    Lock();
    Lock(const Lock&) = delete;
    Lock(Lock&&) noexcept;
    ~Lock();
    Lock& operator=(const Lock&) = delete;
    Lock& operator=(Lock&&) noexcept;

    bool CanBeAcquired(LockType lock_type) {
      return acquired_count == 0 ||
             (queue.empty() && this->lock_mode == LockType::kShared &&
              lock_type == LockType::kShared);
    }

    int acquired_count = 0;
    LockType lock_mode = LockType::kShared;
    std::list<LockRequest> queue;
  };

  void AcquireLock(PartitionedLockRequest request,
                   base::WeakPtr<PartitionedLockHolder> locks_holder,
                   base::OnceClosure acquired_callback);

  void LockReleased(PartitionedLockId lock_id);

  SEQUENCE_CHECKER(sequence_checker_);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::flat_map<PartitionedLockId, Lock> locks_;

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
