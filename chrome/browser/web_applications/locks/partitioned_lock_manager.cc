// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/partitioned_lock_manager.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/web_applications/locks/partitioned_lock.h"
#include "chrome/browser/web_applications/locks/partitioned_lock_id.h"

namespace web_app {
namespace {
void CallIfHolderIsAlive(base::WeakPtr<PartitionedLockHolder> lock_holder,
                         base::OnceClosure on_all_locks_acquired) {
  if (lock_holder) {
    std::move(on_all_locks_acquired).Run();
  }
}
}  // namespace

PartitionedLockHolder::PartitionedLockHolder() = default;
PartitionedLockHolder::~PartitionedLockHolder() = default;

PartitionedLockManager::PartitionedLockRequest::PartitionedLockRequest(
    PartitionedLockId lock_id,
    LockType type)
    : lock_id(std::move(lock_id)), type(type) {}

PartitionedLockManager::LockRequest::LockRequest() = default;
PartitionedLockManager::LockRequest::LockRequest(
    LockType type,
    base::WeakPtr<PartitionedLockHolder> locks_holder,
    base::OnceClosure acquire_next_lock_or_post_completion,
    const base::Location& location)
    : requested_type(type),
      locks_holder(std::move(locks_holder)),
      acquire_next_lock_or_post_completion(
          std::move(acquire_next_lock_or_post_completion)),
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
    PartitionedLockHolder& locks_holder,
    LocksAcquiredCallback callback,
    const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Pre-allocate all locks in the request, so the `locks_` map is not mutated
  // while acquiring locks (which invalidates iterators and storage).
  for (const PartitionedLockRequest& request : lock_requests) {
    locks_.try_emplace(request.lock_id);
    // Ensure that none of the locks are 'before' any already-held locks by the
    // holder.
    for (const PartitionedLock& lock : locks_holder.locks) {
      CHECK(lock.lock_id() < request.lock_id)
          << lock.lock_id() << " vs " << request.lock_id;
    }
  }

  locks_holder.locks.reserve(locks_holder.locks.size() + lock_requests.size());
  auto stored_requests =
      std::make_unique<base::flat_set<PartitionedLockRequest>>(
          std::move(lock_requests));
  auto first_request = stored_requests->begin();

  AcquireNextLockOrPostCompletion(
      std::move(stored_requests), first_request, locks_holder.AsWeakPtr(),
      base::BindOnce(CallIfHolderIsAlive, locks_holder.AsWeakPtr(),
                     std::move(callback)),
      location);
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

base::Value PartitionedLockManager::ToDebugValue(
    TransformLockIdToStringFn transform) const {
  base::Value::Dict result;
  for (const std::pair<PartitionedLockId, Lock>& id_lock_pair : locks_) {
    const Lock& lock = id_lock_pair.second;
    base::Value::Dict lock_state;
    base::Value::List held_locations;
    for (const base::Location& location : lock.request_locations) {
      held_locations.Append(location.ToString());
    }
    lock_state.Set("held_locations", std::move(held_locations));

    base::Value::List queued_locations;
    for (const LockRequest& request : lock.queue) {
      queued_locations.Append(request.location.ToString());
    }
    lock_state.Set("queue", std::move(queued_locations));

    std::string id_as_str = transform(id_lock_pair.first);
    DCHECK(!result.contains(id_as_str))
        << id_as_str << " already exists in " << result.DebugString()
        << ", cannot insert " << lock_state.DebugString();
    result.Set(id_as_str, std::move(lock_state));
  }
  return base::Value(std::move(result));
}

void PartitionedLockManager::AcquireNextLockOrPostCompletion(
    std::unique_ptr<base::flat_set<PartitionedLockRequest>> requests,
    base::flat_set<PartitionedLockRequest>::iterator current,
    base::WeakPtr<PartitionedLockHolder> locks_holder,
    base::OnceClosure on_all_acquired,
    const base::Location& location) {
  if (on_all_acquired.IsCancelled() || on_all_acquired.is_null()) {
    return;
  }
  if (!locks_holder || current == requests->end()) {
    VLOG(1) << "All locks acquired for " << location.ToString();
    // All locks have been acquired or we're aborting.
    task_runner_->PostTask(FROM_HERE, std::move(on_all_acquired));
    return;
  }
  // Note: This function cannot modify the `locks_` map.

  PartitionedLockRequest& request = *current;
  LocksMap::iterator lock_it = locks_.find(request.lock_id);
  CHECK(lock_it != locks_.end());
  Lock& lock = lock_it->second;

  auto acquire_next_lock_or_post_completion = base::BindOnce(
      &PartitionedLockManager::AcquireNextLockOrPostCompletion,
      weak_factory_.GetWeakPtr(), std::move(requests), std::next(current),
      locks_holder, std::move(on_all_acquired), location);

  if (lock.CanBeAcquired(request.type)) {
    VLOG(1) << "Acquiring " << request.lock_id << " for "
            << location.ToString();
    ++lock.acquired_count;
    lock.lock_mode = request.type;
    lock.request_locations.insert(location);
    auto released_callback =
        base::BindOnce(&PartitionedLockManager::LockReleased,
                       weak_factory_.GetWeakPtr(), location);
    locks_holder->locks.emplace_back(std::move(request.lock_id),
                                     std::move(released_callback));
    std::move(acquire_next_lock_or_post_completion).Run();
    return;
  }

  // The lock cannot be acquired now, so we put it on the queue which will
  // grant the given callback the lock when it is acquired in the future in
  // the |LockReleased| method.
  lock.queue.emplace_back(request.type, std::move(locks_holder),
                          std::move(acquire_next_lock_or_post_completion),
                          location);
}

void PartitionedLockManager::LockReleased(base::Location request_location,
                                          PartitionedLockId lock_id) {
  VLOG(1) << "Releasing " << lock_id << " requested by "
          << request_location.ToString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This iterator is guaranteed to stay valid because
  // AcquireNextLockOrPostCompletion does not modify the `locks_` map.
  LocksMap::iterator it = locks_.find(lock_id);
  CHECK(it != locks_.end(), base::NotFatalUntil::M130);
  Lock& lock = it->second;

  // First, decrement the lock `acquired_count`.
  DCHECK_GT(lock.acquired_count, 0);
  --(lock.acquired_count);
  lock.request_locations.erase(request_location);
  if (lock.acquired_count != 0) {
    return;
  }

  // Grant shared locks eagerly and exclusive locks only if `acquired_count` is
  // 0. Note that exclusive locks return below immediately after being granted.
  while (!lock.queue.empty() &&
         (lock.queue.front().requested_type == LockType::kShared ||
          lock.acquired_count == 0)) {
    LockRequest requester = std::move(lock.queue.front());
    lock.queue.pop_front();
    // Skip the request if the lock holder is already destroyed. This
    // avoids stack overflows for long chains of released locks. See
    // https://crbug.com/959743
    if (!requester.locks_holder) {
      continue;
    }
    // Grant the lock.
    VLOG(1) << "Acquiring " << lock_id << " for "
            << requester.location.ToString();
    ++lock.acquired_count;
    lock.lock_mode = requester.requested_type;
    lock.request_locations.insert(requester.location);
    auto released_callback =
        base::BindOnce(&PartitionedLockManager::LockReleased,
                       weak_factory_.GetWeakPtr(), requester.location);
    requester.locks_holder->locks.emplace_back(lock_id,
                                               std::move(released_callback));
    std::move(requester.acquire_next_lock_or_post_completion).Run();
    if (requester.requested_type == LockType::kExclusive) {
      return;
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

}  // namespace web_app
