// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/disjoint_range_lock_manager.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace content {

DisjointRangeLockManager::LockRequest::LockRequest() = default;
DisjointRangeLockManager::LockRequest::LockRequest(
    LockType type,
    base::WeakPtr<ScopesLocksHolder> locks_holder,
    base::OnceClosure acquired_callback)
    : requested_type(type),
      locks_holder(std::move(locks_holder)),
      acquired_callback(std::move(acquired_callback)) {}
DisjointRangeLockManager::LockRequest::LockRequest(LockRequest&&) noexcept =
    default;
DisjointRangeLockManager::LockRequest::~LockRequest() = default;
DisjointRangeLockManager::Lock::Lock() = default;
DisjointRangeLockManager::Lock::Lock(Lock&&) noexcept = default;
DisjointRangeLockManager::Lock::~Lock() = default;
DisjointRangeLockManager::Lock& DisjointRangeLockManager::Lock::operator=(
    DisjointRangeLockManager::Lock&&) noexcept = default;

DisjointRangeLockManager::DisjointRangeLockManager(int level_count)
    : task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  locks_.resize(level_count);
}

DisjointRangeLockManager::~DisjointRangeLockManager() = default;

int64_t DisjointRangeLockManager::LocksHeldForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t locks = 0;
  for (const LockLevelMap& map : locks_) {
    for (const auto& pair : map) {
      locks += pair.second.acquired_count;
    }
  }
  return locks;
}
int64_t DisjointRangeLockManager::RequestsWaitingForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t requests = 0;
  for (const LockLevelMap& map : locks_) {
    for (const auto& pair : map) {
      requests += pair.second.queue.size();
    }
  }
  return requests;
}

bool DisjointRangeLockManager::AcquireLocks(
    base::flat_set<ScopeLockRequest> lock_requests,
    base::WeakPtr<ScopesLocksHolder> locks_holder,
    LocksAquiredCallback callback) {
  if (!locks_holder)
    return false;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Code relies on free locks being granted synchronously. If the locks aren't
  // free, then the grant must happen as a PostTask to avoid overflowing the
  // stack (every task would execute in the stack of its parent task,
  // recursively).
  scoped_refptr<base::RefCountedData<bool>> run_callback_synchonously =
      base::MakeRefCounted<base::RefCountedData<bool>>(true);

  locks_holder->locks.reserve(lock_requests.size());
  size_t num_closure_calls = lock_requests.size();
  base::RepeatingClosure all_locks_acquired_barrier = base::BarrierClosure(
      num_closure_calls,
      base::BindOnce(
          [](scoped_refptr<base::SequencedTaskRunner> runner,
             scoped_refptr<base::RefCountedData<bool>> run_synchronously,
             base::WeakPtr<ScopesLocksHolder> holder,
             LocksAquiredCallback callback) {
            // All locks have been acquired.
            if (!holder || callback.IsCancelled() || callback.is_null())
              return;
            if (run_synchronously->data)
              std::move(callback).Run();
            else
              runner->PostTask(FROM_HERE, std::move(callback));
          },
          task_runner_, run_callback_synchonously, locks_holder,
          std::move(callback)));
  for (ScopeLockRequest& request : lock_requests) {
    bool success = AcquireLock(std::move(request), locks_holder,
                               all_locks_acquired_barrier);
    if (!success) {
      locks_holder->locks.clear();
      return false;
    }
  }
  // If the barrier wasn't run yet, then it will be run asynchronously.
  run_callback_synchonously->data = false;
  return true;
}

void DisjointRangeLockManager::RemoveLockRange(int level,
                                               const ScopeLockRange& range) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(level, static_cast<int>(locks_.size()));
  auto& level_locks = locks_[level];
  auto it = level_locks.find(range);
  if (it != level_locks.end()) {
    DCHECK_EQ(0, it->second.acquired_count);
    level_locks.erase(it);
  }
}

bool DisjointRangeLockManager::AcquireLock(
    ScopeLockRequest request,
    base::WeakPtr<ScopesLocksHolder> locks_holder,
    base::OnceClosure acquired_callback) {
  DCHECK(locks_holder);
  if (request.level < 0 || static_cast<size_t>(request.level) >= locks_.size())
    return false;
  if (request.range.begin >= request.range.end)
    return false;

  auto& level_locks = locks_[request.level];

  auto it = level_locks.find(request.range);
  if (it == level_locks.end()) {
    it = level_locks
             .emplace(std::piecewise_construct,
                      std::forward_as_tuple(std::move(request.range)),
                      std::forward_as_tuple())
             .first;
  }

  if (!IsRangeDisjointFromNeighbors(level_locks, request.range)) {
    level_locks.erase(it);
    return false;
  }

  Lock& lock = it->second;
  if (lock.CanBeAcquired(request.type)) {
    ++lock.acquired_count;
    lock.lock_mode = request.type;
    auto released_callback = base::BindOnce(
        &DisjointRangeLockManager::LockReleased, weak_factory_.GetWeakPtr());
    locks_holder->locks.emplace_back(std::move(request.range), request.level,
                                     std::move(released_callback));
    std::move(acquired_callback).Run();
  } else {
    // The lock cannot be acquired now, so we put it on the queue which will
    // grant the given callback the lock when it is acquired in the future in
    // the |LockReleased| method.
    lock.queue.emplace_back(request.type, std::move(locks_holder),
                            std::move(acquired_callback));
  }
  return true;
}

void DisjointRangeLockManager::LockReleased(int level, ScopeLockRange range) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(level, static_cast<int>(locks_.size()));
  auto& level_locks = locks_[level];
  auto it = level_locks.find(range);
  DCHECK(it != level_locks.end());
  Lock& lock = it->second;

  DCHECK_GT(lock.acquired_count, 0);
  --(lock.acquired_count);
  if (lock.acquired_count == 0) {
    // Either the lock isn't acquired yet or more shared locks can be granted.
    while (!lock.queue.empty() &&
           (lock.acquired_count == 0 ||
            lock.queue.front().requested_type == LockType::kShared)) {
      // Pop the next requester.
      LockRequest requester = std::move(lock.queue.front());
      lock.queue.pop_front();

      // Skip the request if the lock holder is already destroyed. This avoids
      // stack overflows for long chains of released locks.
      // See https://crbug.com/959743
      if (!requester.locks_holder) {
        continue;
      }

      ++lock.acquired_count;
      lock.lock_mode = requester.requested_type;
      auto released_callback = base::BindOnce(
          &DisjointRangeLockManager::LockReleased, weak_factory_.GetWeakPtr());
      // Grant the lock.
      requester.locks_holder->locks.emplace_back(std::move(range), level,
                                                 std::move(released_callback));
      std::move(requester.acquired_callback).Run();
      // This can only happen if acquired_count was 0.
      if (requester.requested_type == LockType::kExclusive)
        return;
    }
  }
}

// static
bool DisjointRangeLockManager::IsRangeDisjointFromNeighbors(
    const LockLevelMap& map,
    const ScopeLockRange& range) {
  DCHECK_EQ(map.count(range), 1ull);
  auto it = map.find(range);
  auto next_it = it;
  ++next_it;
  if (next_it != map.end()) {
    return range.end <= next_it->first.begin;
  }
  auto last_it = it;
  if (last_it != map.begin()) {
    --last_it;
    return last_it->first.end <= range.begin;
  }
  return true;
}

}  // namespace content
