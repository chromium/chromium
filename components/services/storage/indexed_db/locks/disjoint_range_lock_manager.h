// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_DISJOINT_RANGE_LOCK_MANAGER_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_DISJOINT_RANGE_LOCK_MANAGER_H_

#include <list>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/indexed_db/locks/leveled_lock_manager.h"

namespace content {

// Holds locks of the scopes system. To be performant without an Interval Tree,
// this implementation has the following invariants:
// * All lock range requests at a level must be disjoint - they cannot overlap.
// * Lock ranges are remembered for future performance - remove them using
//   RemoveLockRange.
// * Lock ranges are compared using bytewise comparison.
//
// Additional invariants for this implementation:
// * All calls must happen from the same sequenced task runner.
// * Locks are granted in the order in which they are requested.
// * Locks held by an entity must be acquired all at once. If more locks are
//   needed (where old locks will continue to be held), then all locks must be
//   released first, and then all necessary locks acquired in one acquisition
//   call.
class COMPONENT_EXPORT(LOCK_MANAGER) DisjointRangeLockManager
    : public LeveledLockManager {
 public:
  // Creates a lock manager with the given number of levels, the comparator for
  // leveldb keys, and the current task runner that we are running on. The task
  // runner will be used for the lock acquisition callbacks.
  explicit DisjointRangeLockManager(int level_count);

  DisjointRangeLockManager(const DisjointRangeLockManager&) = delete;
  DisjointRangeLockManager& operator=(const DisjointRangeLockManager&) = delete;

  ~DisjointRangeLockManager() override;

  int64_t LocksHeldForTesting() const override;
  int64_t RequestsWaitingForTesting() const override;

  // Returns if the request was valid. To be valid, all requests must have:
  // * lock level < |level_count| populated above,
  // * |range.begin| < |range.end| using the |comparator| above,
  // * range disjoint from other lock ranges (which is an implementation
  //   invariant).
  bool AcquireLocks(base::flat_set<LeveledLockRequest> lock_requests,
                    base::WeakPtr<LeveledLockHolder> locks_holder,
                    LocksAcquiredCallback callback) override;

  // Remove the given lock range at the given level. The lock range must not be
  // in use. Use this if the lock will never be used again.
  void RemoveLockRange(int level, const LeveledLockRange& range);

 private:
  struct LockRequest {
   public:
    LockRequest();
    LockRequest(LockRequest&&) noexcept;
    LockRequest(LockType type,
                base::WeakPtr<LeveledLockHolder> locks_holder,
                base::OnceClosure callback);
    ~LockRequest();

    LockType requested_type = LockType::kShared;
    base::WeakPtr<LeveledLockHolder> locks_holder;
    base::OnceClosure acquired_callback;
  };

  // Represents a lock, which has a range and a level. To support shared access,
  // there can be multiple acquisitions of this lock, represented in
  // |acquired_count|. Also holds the pending requests for this lock.
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

  using LockLevelMap = base::flat_map<LeveledLockRange, Lock>;

  bool AcquireLock(LeveledLockRequest request,
                   base::WeakPtr<LeveledLockHolder> locks_holder,
                   base::OnceClosure acquired_callback);

  void LockReleased(int level, LeveledLockRange range);

  static bool IsRangeDisjointFromNeighbors(const LockLevelMap& map,
                                           const LeveledLockRange& range);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // This vector should never be modified after construction.
  std::vector<LockLevelMap> locks_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DisjointRangeLockManager> weak_factory_{this};
};

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_DISJOINT_RANGE_LOCK_MANAGER_H_
