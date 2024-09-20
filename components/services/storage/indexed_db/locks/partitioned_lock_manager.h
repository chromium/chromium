// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_MANAGER_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_MANAGER_H_

#include <deque>
#include <iosfwd>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"

namespace content::indexed_db {

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

  void CancelLockRequest();

  std::vector<PartitionedLock> locks;
  base::OnceClosure on_cancel;
  base::WeakPtrFactory<PartitionedLockHolder> weak_factory{this};
};

// Holds locks of the scopes system. Granted locks are represented by the
// |PartitionedLock| class.
//
//  Invariants for the lock management system:
// * Locks are granted in the order in which they are requested.
// * Locks held by an entity must be acquired all at once. If more locks are
//   needed (where old locks will continue to be held), then all locks must be
//   released first, and then all necessary locks acquired in one acquisition
//   call.
class PartitionedLockManager {
 public:
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
  struct PartitionedLockRequest {
    PartitionedLockRequest(PartitionedLockId lock_id, LockType type);
    PartitionedLockId lock_id;
    LockType type;
  };
  // Acquires locks for the given requests.
  // `compare_priority` must return true iff this acquisition has a *higher*
  // priority than the one represented by the passed holder, and should skip
  // ahead of it in line (`request_queue_`). This property need not be
  // transitive. May be null, which is equivalent to always returning false.
  void AcquireLocks(base::flat_set<PartitionedLockRequest> lock_requests,
                    PartitionedLockHolder& locks_holder,
                    base::OnceClosure callback,
                    base::RepeatingCallback<bool(const PartitionedLockHolder&)>
                        compare_priority = {});

  enum class TestLockResult { kLocked, kFree };
  // Tests to see if the given lock request can be acquired.
  TestLockResult TestLock(PartitionedLockRequest lock_requests);

  // Filter out the list of `PartitionedLockId`s that cannot be acquired given
  // the list of `PartitionedLockRequest`.
  // See `Lock::CanBeAcquired()`.
  std::vector<PartitionedLockId> GetUnacquirableLocks(
      std::vector<PartitionedLockRequest>& lock_requests);

  // Returns the lock requests that are blocked on the provided lock holder.
  std::set<PartitionedLockHolder*> GetBlockedRequests(
      const base::flat_set<PartitionedLockId>& held_locks) const;

  // Outputs the lock state (held & requested locks) into a debug value,
  // suitable for printing an 'internals' or to print during debugging. The
  // `transform` is used to change the lock ids to human-readable values.
  // Note: The human-readable values MUST be unique per lock id, and if to lock
  // ids resolve to the same string, then this function will DCHECK.
  using TransformLockIdToStringFn = std::string(const PartitionedLockId&);

 private:
  // Metadata representing the state of a lockable entity, which is in turn
  // defined by an ID (`PartitionedLockId`). To support shared access, there can
  // be multiple acquisitions of this lock, represented in |acquired_count|.
  struct LockState {
    LockState();
    LockState(const LockState&) = delete;
    LockState(LockState&&) noexcept;
    ~LockState();
    LockState& operator=(const LockState&) = delete;
    LockState& operator=(LockState&&) noexcept;

    bool CanBeAcquired(LockType lock_type) {
      return acquired_count == 0 || (this->access_mode == LockType::kShared &&
                                     lock_type == LockType::kShared);
    }

    // The number of holders sharing the lock.
    int acquired_count = 0;

    // The current access mode. If kExclusive, `acquired_count` must not be more
    // than 1. If `acquired_count` is zero, this is meaningless.
    LockType access_mode = LockType::kShared;
  };

  // Represents a request to grab a number of locks.
  struct AcquisitionRequest {
    AcquisitionRequest();
    ~AcquisitionRequest();

    AcquisitionRequest(AcquisitionRequest&&);
    AcquisitionRequest& operator=(AcquisitionRequest&&) = default;

    // To be called when the locks are all acquired.
    base::OnceClosure acquired_callback;

    // The entities that the request seeks to lock.
    base::flat_set<PartitionedLockRequest> lock_requests;

    // Ownership of the locks will be transferred to this object.
    base::WeakPtr<PartitionedLockHolder> locks_holder;
  };

  // Returns true if the requests are overlapping, i.e. they couldn't
  // simultaneously be filled.
  static bool RequestsAreOverlapping(
      const base::flat_set<PartitionedLockRequest>& requests_a,
      const base::flat_set<PartitionedLockRequest>& requests_b);

  // If locks can be granted to the requester at `requests_iter`, then grant
  // those locks and remove the requester from `request_queue_`, returning an
  // iter pointing to the next request after where the old one was in the list.
  // If locks can't be granted, return an iterator pointing to the next request
  // in the queue (i.e. one after `requests_iter`).
  std::list<AcquisitionRequest>::iterator MaybeGrantLocksAndIterate(
      std::list<AcquisitionRequest>::iterator requests_iter,
      bool notify_synchronously = false);

  void AcquireLock(PartitionedLockRequest request,
                   PartitionedLockHolder& locks_holder);

  bool CanAcquireLock(PartitionedLockId lock_id, LockType type);

  // Called when an acquisition request is to be dropped. This corresponds to
  // `AcquisitionRequest::locks_holder` becoming null.
  void LockRequestCancelled();

  // Called when a granted lock has been released.
  void LockReleased(PartitionedLockId lock_id);

  // The set of all known lockable entities and their current state.
  std::map<PartitionedLockId, LockState> locks_;

  // This queue is FIFO by default, but some requests may cut ahead in line as
  // determined by `compare_priority`.
  std::list<AcquisitionRequest> request_queue_;

  base::WeakPtrFactory<PartitionedLockManager> weak_factory_{this};
};

bool operator<(const PartitionedLockManager::PartitionedLockRequest& x,
               const PartitionedLockManager::PartitionedLockRequest& y);
bool operator==(const PartitionedLockManager::PartitionedLockRequest& x,
                const PartitionedLockManager::PartitionedLockRequest& y);
bool operator!=(const PartitionedLockManager::PartitionedLockRequest& x,
                const PartitionedLockManager::PartitionedLockRequest& y);

}  // namespace content::indexed_db

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_MANAGER_H_
