// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"

namespace content {

PartitionedLockHolder::PartitionedLockHolder() = default;
PartitionedLockHolder::~PartitionedLockHolder() = default;

PartitionedLockManager::PartitionedLockRequest::PartitionedLockRequest(
    PartitionedLockId lock_id,
    LockType type)
    : lock_id(std::move(lock_id)), type(type) {}

PartitionedLockManager::AcquireOptions::AcquireOptions() = default;

PartitionedLockManager::LockRequest::LockRequest() = default;
PartitionedLockManager::LockRequest::LockRequest(
    LockType type,
    base::WeakPtr<PartitionedLockHolder> locks_holder,
    base::OnceClosure acquired_callback,
    const base::Location& location)
    : requested_type(type),
      locks_holder(std::move(locks_holder)),
      acquired_callback(std::move(acquired_callback)),
      location(location) {}
PartitionedLockManager::LockRequest::LockRequest(LockRequest&&) noexcept =
    default;
PartitionedLockManager::LockRequest::~LockRequest() = default;
PartitionedLockManager::Lock::Lock() = default;
PartitionedLockManager::Lock::Lock(Lock&&) noexcept = default;
PartitionedLockManager::Lock::~Lock() = default;
PartitionedLockManager::Lock& PartitionedLockManager::Lock::operator=(
    PartitionedLockManager::Lock&&) noexcept = default;

PartitionedLockManager::PartitionedLockManager()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

PartitionedLockManager::~PartitionedLockManager() = default;

int64_t PartitionedLockManager::LocksHeldForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t locks = 0;
  for (const auto& [lock_id, lock] : locks_) {
    locks += lock.acquired_count;
  }
  return locks;
}

int64_t PartitionedLockManager::RequestsWaitingForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t requests = 0;
  for (const auto& [lock_id, lock] : locks_) {
    requests += lock.queue.size();
  }
  return requests;
}

void PartitionedLockManager::AcquireLocks(
    base::flat_set<PartitionedLockRequest> lock_requests,
    base::WeakPtr<PartitionedLockHolder> locks_holder,
    LocksAcquiredCallback callback,
    AcquireOptions acquire_options,
    const base::Location& location) {
  if (!locks_holder) {
    std::move(callback).Run();
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Code relies on free locks being granted synchronously. If the locks aren't
  // free, then the grant must happen as a PostTask to avoid overflowing the
  // stack (every task would execute in the stack of its parent task,
  // recursively).
  scoped_refptr<base::RefCountedData<bool>> run_callback_synchonously =
      base::MakeRefCounted<base::RefCountedData<bool>>(
          !acquire_options.ensure_async);

  locks_holder->locks.reserve(lock_requests.size());
  size_t num_closure_calls = lock_requests.size();
  base::RepeatingClosure all_locks_acquired_barrier = base::BarrierClosure(
      num_closure_calls,
      base::BindOnce(
          [](scoped_refptr<base::SequencedTaskRunner> runner,
             scoped_refptr<base::RefCountedData<bool>> run_synchronously,
             base::WeakPtr<PartitionedLockHolder> holder,
             LocksAcquiredCallback callback) {
            // All locks have been acquired.
            if (!holder || callback.IsCancelled() || callback.is_null())
              return;
            if (run_synchronously->data) {
              std::move(callback).Run();
            } else {
              runner->PostTask(FROM_HERE, std::move(callback));
            }
          },
          task_runner_, run_callback_synchonously, locks_holder,
          std::move(callback)));
  for (PartitionedLockRequest& request : lock_requests) {
    AcquireLock(std::move(request), locks_holder, all_locks_acquired_barrier,
                location);
  }
  // If the barrier wasn't run yet, then it will be run asynchronously.
  run_callback_synchonously->data = false;
}

PartitionedLockManager::TestLockResult PartitionedLockManager::TestLock(
    PartitionedLockRequest request) {
  Lock& lock = locks_[request.lock_id];
  return lock.CanBeAcquired(request.type) ? TestLockResult::kFree
                                          : TestLockResult::kLocked;
}

std::vector<base::Location>
PartitionedLockManager::GetHeldAndQueuedLockLocations(
    const base::flat_set<PartitionedLockRequest>& requests) const {
  std::vector<base::Location> result;
  for (const auto& request : requests) {
    auto lock_it = locks_.find(request.lock_id);
    if (lock_it == locks_.end()) {
      continue;
    }
    result.insert(result.end(), lock_it->second.request_locations.begin(),
                  lock_it->second.request_locations.end());
    for (const LockRequest& queued_request : lock_it->second.queue) {
      result.push_back(queued_request.location);
    }
  }
  return result;
}

std::vector<PartitionedLockId> PartitionedLockManager::GetUnacquirableLocks(
    std::vector<PartitionedLockRequest>& lock_requests) {
  std::vector<PartitionedLockId> lock_ids;
  for (PartitionedLockRequest& request : lock_requests) {
    auto it = locks_.find(request.lock_id);
    if (it != locks_.end()) {
      Lock& lock = it->second;
      if (!lock.CanBeAcquired(request.type)) {
        lock_ids.push_back(request.lock_id);
      }
    }
  }
  return lock_ids;
}

void PartitionedLockManager::RemoveLockId(const PartitionedLockId& lock_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = locks_.find(lock_id);
  if (it != locks_.end()) {
    DCHECK_EQ(0, it->second.acquired_count);
    locks_.erase(it);
  }
}

void PartitionedLockManager::AcquireLock(
    PartitionedLockRequest request,
    base::WeakPtr<PartitionedLockHolder> locks_holder,
    base::OnceClosure acquired_callback,
    const base::Location& location) {
  DCHECK(locks_holder);

  auto it = locks_.find(request.lock_id);
  if (it == locks_.end()) {
    it = locks_
             .emplace(std::piecewise_construct,
                      std::forward_as_tuple(request.lock_id),
                      std::forward_as_tuple())
             .first;
  }

  Lock& lock = it->second;
  if (lock.CanBeAcquired(request.type)) {
    ++lock.acquired_count;
    lock.lock_mode = request.type;
    lock.request_locations.insert(location);
    auto released_callback =
        base::BindOnce(&PartitionedLockManager::LockReleased,
                       weak_factory_.GetWeakPtr(), location);
    locks_holder->locks.emplace_back(std::move(request.lock_id),
                                     std::move(released_callback));
    std::move(acquired_callback).Run();
  } else {
    // The lock cannot be acquired now, so we put it on the queue which will
    // grant the given callback the lock when it is acquired in the future in
    // the |LockReleased| method.
    lock.queue.emplace_back(request.type, std::move(locks_holder),
                            std::move(acquired_callback), location);
  }
}

void PartitionedLockManager::LockReleased(base::Location request_location,
                                          PartitionedLockId lock_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = locks_.find(lock_id);
  DCHECK(it != locks_.end());
  Lock& lock = it->second;

  DCHECK_GT(lock.acquired_count, 0);
  --(lock.acquired_count);
  lock.request_locations.erase(request_location);
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
      lock.request_locations.insert(requester.location);
      auto released_callback =
          base::BindOnce(&PartitionedLockManager::LockReleased,
                         weak_factory_.GetWeakPtr(), requester.location);
      // Grant the lock.
      requester.locks_holder->locks.emplace_back(lock_id,
                                                 std::move(released_callback));
      std::move(requester.acquired_callback).Run();
      // This can only happen if acquired_count was 0.
      if (requester.requested_type == LockType::kExclusive)
        return;
    }
  }
}

int64_t PartitionedLockManager::GetQueuedLockRequestCount(
    const PartitionedLockId& lock_id) const {
  int64_t count = 0;

  auto it = locks_.find(lock_id);
  if (it == locks_.end()) {
    return count;
  }

  for (const LockRequest& requester : it->second.queue) {
    if (requester.locks_holder) {
      count++;
    }
  }
  return count;
}

bool operator<(const PartitionedLockManager::PartitionedLockRequest& x,
               const PartitionedLockManager::PartitionedLockRequest& y) {
  if (x.lock_id != y.lock_id)
    return x.lock_id < y.lock_id;
  return x.type < y.type;
}

bool operator==(const PartitionedLockManager::PartitionedLockRequest& x,
                const PartitionedLockManager::PartitionedLockRequest& y) {
  return x.lock_id == y.lock_id && x.type == y.type;
}

bool operator!=(const PartitionedLockManager::PartitionedLockRequest& x,
                const PartitionedLockManager::PartitionedLockRequest& y) {
  return !(x == y);
}

}  // namespace content
