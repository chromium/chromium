// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"

namespace content::indexed_db {

PartitionedLockHolder::PartitionedLockHolder() = default;
PartitionedLockHolder::~PartitionedLockHolder() = default;

void PartitionedLockHolder::CancelLockRequest() {
  weak_factory.InvalidateWeakPtrs();
}

PartitionedLockManager::PartitionedLockRequest::PartitionedLockRequest(
    PartitionedLockId lock_id,
    LockType type)
    : lock_id(std::move(lock_id)), type(type) {}

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

PartitionedLockManager::PartitionedLockManager() = default;
PartitionedLockManager::~PartitionedLockManager() = default;

int64_t PartitionedLockManager::LocksHeldForTesting() const {
  int64_t locks = 0;
  for (const auto& [_, lock] : locks_) {
    locks += lock.acquired_count;
  }
  return locks;
}

int64_t PartitionedLockManager::RequestsWaitingForMetrics() const {
  int64_t requests = 0;
  for (const auto& [_, lock] : locks_) {
    requests += lock.queue.size();
  }
  return requests;
}

int64_t PartitionedLockManager::RequestsWaitingForTesting() const {
  int64_t requests = 0;
  for (const auto& [_, lock] : locks_) {
    requests += lock.queue.size();
  }
  return requests;
}

void PartitionedLockManager::AcquireLocks(
    base::flat_set<PartitionedLockRequest> lock_requests,
    PartitionedLockHolder& locks_holder,
    LocksAcquiredCallback callback,
    const base::Location& location) {
  // Code relies on free locks being granted synchronously. If the locks aren't
  // free, then the grant must happen as a PostTask to avoid overflowing the
  // stack (every task would execute in the stack of its parent task,
  // recursively).
  scoped_refptr<base::RefCountedData<bool>> run_callback_synchronously =
      base::MakeRefCounted<base::RefCountedData<bool>>(true);

  locks_holder.locks.reserve(lock_requests.size());
  size_t num_closure_calls = lock_requests.size();
  base::RepeatingClosure all_locks_acquired_barrier = base::BarrierClosure(
      num_closure_calls,
      base::BindOnce(
          [](scoped_refptr<base::RefCountedData<bool>> run_synchronously,
             base::WeakPtr<PartitionedLockHolder> holder,
             LocksAcquiredCallback callback) {
            // All locks have been acquired.
            if (!holder || callback.IsCancelled() || callback.is_null()) {
              return;
            }
            if (run_synchronously->data) {
              std::move(callback).Run();
            } else {
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, std::move(callback));
            }
          },
          run_callback_synchronously, locks_holder.weak_factory.GetWeakPtr(),
          std::move(callback)));
  for (PartitionedLockRequest& request : lock_requests) {
    AcquireLock(std::move(request), locks_holder, all_locks_acquired_barrier,
                location);
  }
  // If the barrier wasn't run yet, then it will be run asynchronously.
  run_callback_synchronously->data = false;
}

PartitionedLockManager::TestLockResult PartitionedLockManager::TestLock(
    PartitionedLockRequest request) {
  Lock& lock = locks_[request.lock_id];
  return lock.CanBeAcquired(request.type) ? TestLockResult::kFree
                                          : TestLockResult::kLocked;
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

bool PartitionedLockManager::IsBlockingAnyRequest(
    const base::flat_set<PartitionedLockId>& held_locks,
    base::FunctionRef<bool(const PartitionedLockHolder&)> filter) const {
  for (const PartitionedLockId& lock_id : held_locks) {
    auto it = locks_.find(lock_id);
    CHECK(it != locks_.end());

    for (const LockRequest& request : it->second.queue) {
      if (request.locks_holder && filter(*request.locks_holder)) {
        return true;
      }
    }
  }

  return false;
}

void PartitionedLockManager::RemoveLockId(const PartitionedLockId& lock_id) {
  auto it = locks_.find(lock_id);
  if (it != locks_.end()) {
    CHECK_EQ(0, it->second.acquired_count);
    locks_.erase(it);
  }
}

base::Value PartitionedLockManager::ToDebugValue(
    TransformLockIdToStringFn transform) const {
  base::DictValue result;
  for (const std::pair<PartitionedLockId, Lock>& id_lock_pair : locks_) {
    const Lock& lock = id_lock_pair.second;
    base::DictValue lock_state;
    base::ListValue held_locations;
    for (const base::Location& location : lock.request_locations) {
      held_locations.Append(location.ToString());
    }
    lock_state.Set("held_locations", std::move(held_locations));

    base::ListValue queued_locations;
    for (const LockRequest& request : lock.queue) {
      queued_locations.Append(request.location.ToString());
    }
    lock_state.Set("queue", std::move(queued_locations));

    std::string id_as_str = transform(id_lock_pair.first);
    CHECK(!result.contains(id_as_str))
        << id_as_str << " already exists in " << result.DebugString()
        << ", cannot insert " << lock_state.DebugString();
    result.Set(id_as_str, std::move(lock_state));
  }
  return base::Value(std::move(result));
}

void PartitionedLockManager::AcquireLock(PartitionedLockRequest request,
                                         PartitionedLockHolder& locks_holder,
                                         base::OnceClosure acquired_callback,
                                         const base::Location& location) {
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
    locks_holder.locks.emplace_back(std::move(request.lock_id),
                                    std::move(released_callback));
    std::move(acquired_callback).Run();
  } else {
    // The lock cannot be acquired now, so we put it on the queue which will
    // grant the given callback the lock when it is acquired in the future in
    // the |LockReleased| method.
    lock.queue.emplace_back(request.type,
                            locks_holder.weak_factory.GetWeakPtr(),
                            std::move(acquired_callback), location);
  }
}

void PartitionedLockManager::LockReleased(base::Location request_location,
                                          PartitionedLockId lock_id) {
  auto it = locks_.find(lock_id);
  CHECK(it != locks_.end());
  Lock& lock = it->second;

  CHECK_GT(lock.acquired_count, 0);
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

      // Skip the request if the lock holder is already destroyed.
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
      if (requester.requested_type == LockType::kExclusive) {
        return;
      }
    }
  }
}

std::set<PartitionedLockHolder*> PartitionedLockManager::GetQueuedRequests(
    const PartitionedLockId& lock_id) const {
  std::set<PartitionedLockHolder*> blocked_requests;

  auto it = locks_.find(lock_id);
  if (it == locks_.end()) {
    return blocked_requests;
  }

  for (const LockRequest& request : it->second.queue) {
    if (request.locks_holder) {
      blocked_requests.insert(request.locks_holder.get());
    }
  }
  return blocked_requests;
}

bool operator<(const PartitionedLockManager::PartitionedLockRequest& x,
               const PartitionedLockManager::PartitionedLockRequest& y) {
  if (x.lock_id != y.lock_id) {
    return x.lock_id < y.lock_id;
  }
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

}  // namespace content::indexed_db
