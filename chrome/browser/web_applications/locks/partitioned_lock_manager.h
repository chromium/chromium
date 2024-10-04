// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_PARTITIONED_LOCK_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_PARTITIONED_LOCK_MANAGER_H_

#include <deque>
#include <iosfwd>
#include <list>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/web_applications/locks/partitioned_lock.h"
#include "chrome/browser/web_applications/locks/partitioned_lock_id.h"

namespace base {
class Value;
}

namespace web_app {

// Used to receive and hold locks from a PartitionedLockManager. This struct
// enables the PartitionedLock objects to always live in the destination of the
// caller's choosing (as opposed to having the locks be an argument in the
// callback, where they could be owned by the task scheduler).
//
// This class must be used and destructed on the same sequence as the
// PartitionedLockManager.
struct PartitionedLockHolder : public base::SupportsUserData {
  PartitionedLockHolder();
  PartitionedLockHolder(const PartitionedLockHolder&) = delete;
  PartitionedLockHolder& operator=(const PartitionedLockHolder&) = delete;
  ~PartitionedLockHolder() override;

  base::WeakPtr<PartitionedLockHolder> AsWeakPtr() {
    return weak_factory.GetWeakPtr();
  }

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
class PartitionedLockManager {
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

  struct PartitionedLockRequest {
    PartitionedLockRequest(PartitionedLockId lock_id, LockType type);
    PartitionedLockId lock_id;
    LockType type;
  };
  // Acquires locks for the given requests. Lock partitions are treated as
  // completely independent domains. The request is aborted and the `callback`
  // is not called if `locks_holder` is destroyed.
  // The `callback` is guaranteed to be called asynchronously when all locks are
  // granted. Locks are requested and granted in order according to the sorting
  // order of `lock_id` in the request, where requesting the next lock does not
  // occur until the previous lock is granted.
  void AcquireLocks(base::flat_set<PartitionedLockRequest> lock_requests,
                    PartitionedLockHolder& locks_holder,
                    LocksAcquiredCallback callback,
                    const base::Location& location = FROM_HERE);

  enum class TestLockResult { kLocked, kFree };
  // Tests to see if the given lock request can be acquired.
  TestLockResult TestLock(PartitionedLockRequest lock_requests);

  // Gets the request location of all locks currently held and queued for the
  // given requests.
  std::vector<base::Location> GetHeldAndQueuedLockLocations(
      const base::flat_set<PartitionedLockRequest>& requests) const;

  // Filter out the list of `PartitionedLockId`s that cannot be acquired given
  // the list of `PartitionedLockRequest`.
  // See `Lock::CanBeAcquired()`.
  std::vector<PartitionedLockId> GetUnacquirableLocks(
      std::vector<PartitionedLockRequest>& lock_requests);

  // Returns the lock requests that are blocked on the provided `lock_id`.
  std::set<PartitionedLockHolder*> GetQueuedRequests(
      const PartitionedLockId& lock_id) const;

  // Outputs the lock state (held & requested locks) into a debug value,
  // suitable for printing an 'internals' or to print during debugging. The
  // `transform` is used to change the lock ids to human-readable values.
  // Note: The human-readable values MUST be unique per lock id, and if to lock
  // ids resolve to the same string, then this function will DCHECK.
  using TransformLockIdToStringFn = std::string(const PartitionedLockId&);
  base::Value ToDebugValue(TransformLockIdToStringFn transform) const;

 private:
  struct LockRequest {
   public:
    LockRequest();
    LockRequest(LockRequest&&) noexcept;
    LockRequest(LockType type,
                base::WeakPtr<PartitionedLockHolder> locks_holder,
                base::OnceClosure acquire_next_lock_or_post_completion,
                const base::Location& location);
    ~LockRequest();

    LockType requested_type = LockType::kShared;
    base::WeakPtr<PartitionedLockHolder> locks_holder;
    base::OnceClosure acquire_next_lock_or_post_completion;
    base::Location location;
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
    base::flat_set<base::Location> request_locations;
    LockType lock_mode = LockType::kShared;
    std::list<LockRequest> queue;
  };
  using LocksMap = base::flat_map<PartitionedLockId, Lock>;

  // This must not add or remove from the `locks_` storage.
  void AcquireNextLockOrPostCompletion(
      std::unique_ptr<base::flat_set<PartitionedLockRequest>> requests,
      base::flat_set<PartitionedLockRequest>::iterator current,
      base::WeakPtr<PartitionedLockHolder> locks_holder,
      base::OnceClosure on_all_acquired,
      const base::Location& location);

  void LockReleased(base::Location request_location, PartitionedLockId lock_id);

  SEQUENCE_CHECKER(sequence_checker_);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  LocksMap locks_;

  base::WeakPtrFactory<PartitionedLockManager> weak_factory_{this};
};

bool operator<(const PartitionedLockManager::PartitionedLockRequest& x,
               const PartitionedLockManager::PartitionedLockRequest& y);
bool operator==(const PartitionedLockManager::PartitionedLockRequest& x,
                const PartitionedLockManager::PartitionedLockRequest& y);
bool operator!=(const PartitionedLockManager::PartitionedLockRequest& x,
                const PartitionedLockManager::PartitionedLockRequest& y);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_PARTITIONED_LOCK_MANAGER_H_
