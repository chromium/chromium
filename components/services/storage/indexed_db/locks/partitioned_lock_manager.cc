// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

#include <cstddef>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"

namespace content::indexed_db {

PartitionedLockHolder::PartitionedLockHolder() = default;

PartitionedLockHolder::~PartitionedLockHolder() {
  CancelLockRequest();
}

void PartitionedLockHolder::CancelLockRequest() {
  weak_factory.InvalidateWeakPtrs();
  if (on_cancel) {
    std::move(on_cancel).Run();
  }
}

PartitionedLockManager::PartitionedLockRequest::PartitionedLockRequest(
    PartitionedLockId lock_id,
    LockType type)
    : lock_id(std::move(lock_id)), type(type) {}

PartitionedLockManager::AcquisitionRequest::AcquisitionRequest() = default;
PartitionedLockManager::AcquisitionRequest::~AcquisitionRequest() = default;
PartitionedLockManager::AcquisitionRequest::AcquisitionRequest(
    AcquisitionRequest&&) = default;

PartitionedLockManager::LockState::LockState() = default;
PartitionedLockManager::LockState::LockState(LockState&&) noexcept = default;
PartitionedLockManager::LockState::~LockState() = default;
PartitionedLockManager::LockState& PartitionedLockManager::LockState::operator=(
    PartitionedLockManager::LockState&&) noexcept = default;

PartitionedLockManager::PartitionedLockManager() = default;
PartitionedLockManager::~PartitionedLockManager() = default;

int64_t PartitionedLockManager::LocksHeldForTesting() const {
  int64_t locks = 0;
  for (const auto& [_, lock] : locks_) {
    locks += lock.acquired_count;
  }
  return locks;
}

int64_t PartitionedLockManager::RequestsWaitingForTesting() const {
  return request_queue_.size();
}

void PartitionedLockManager::AcquireLocks(
    base::flat_set<PartitionedLockRequest> lock_requests,
    PartitionedLockHolder& locks_holder,
    base::OnceClosure acquired_callback,
    base::RepeatingCallback<bool(const PartitionedLockHolder&)>
        compare_priority) {
  locks_holder.on_cancel =
      base::BindOnce(&PartitionedLockManager::LockRequestCancelled,
                     weak_factory_.GetWeakPtr());

  // Insert according to priority.
  auto iter = request_queue_.rbegin();
  if (compare_priority) {
    while (iter != request_queue_.rend() &&
           compare_priority.Run(*iter->locks_holder)) {
      ++iter;
    }
  }

  auto new_request = request_queue_.emplace(iter.base());
  new_request->acquired_callback = std::move(acquired_callback);
  new_request->lock_requests = std::move(lock_requests);
  new_request->locks_holder = locks_holder.weak_factory.GetWeakPtr();

  // Free locks are granted synchronously. If the request is enqueued, then the
  // callback will eventually be run asynchronously to avoid reentrancy. See
  // crbug.com/40626055
  MaybeGrantLocksAndIterate(new_request, true);
}

// static
bool PartitionedLockManager::RequestsAreOverlapping(
    const base::flat_set<PartitionedLockRequest>& requests_a,
    const base::flat_set<PartitionedLockRequest>& requests_b) {
  return base::ranges::any_of(
      requests_a, [&requests_b](const PartitionedLockRequest& lock_request_a) {
        return base::ranges::any_of(
            requests_b,
            [&lock_request_a](const PartitionedLockRequest& lock_request_b) {
              return (lock_request_a.type == LockType::kExclusive ||
                      lock_request_b.type == LockType::kExclusive) &&
                     lock_request_a.lock_id == lock_request_b.lock_id;
            });
      });
}

std::list<PartitionedLockManager::AcquisitionRequest>::iterator
PartitionedLockManager::MaybeGrantLocksAndIterate(
    std::list<AcquisitionRequest>::iterator requests_iter,
    bool notify_synchronously) {
  // Do nothing if we can't grant *every* lock. Note that it's important this is
  // `any_of` and not `all_of` (with an inverted predicate) in order to support
  // empty lock requests.
  if (base::ranges::any_of(requests_iter->lock_requests,
                           [this](PartitionedLockRequest& request) {
                             auto it = locks_.find(request.lock_id);
                             if (it == locks_.end()) {
                               return false;
                             }

                             return !it->second.CanBeAcquired(request.type);
                           })) {
    return ++requests_iter;
  }

  // Don't do anything if there are overlapping requests ahead in the queue.
  for (auto iter = request_queue_.begin(); iter != requests_iter; ++iter) {
    if (RequestsAreOverlapping(requests_iter->lock_requests,
                               iter->lock_requests)) {
      return ++requests_iter;
    }
  }

  // Erase the request from the queue before acquiring the locks.
  AcquisitionRequest acquisition_request(std::move(*requests_iter));
  auto next_iter = request_queue_.erase(requests_iter);

  for (PartitionedLockRequest& request : acquisition_request.lock_requests) {
    AcquireLock(request, *acquisition_request.locks_holder);
  }

  if (notify_synchronously) {
    std::move(acquisition_request.acquired_callback).Run();
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(acquisition_request.acquired_callback));
  }

  return next_iter;
}

bool PartitionedLockManager::CanAcquireLock(PartitionedLockId lock_id,
                                            LockType type) {
  auto it = locks_.find(lock_id);
  if (it == locks_.end()) {
    return true;
  }

  return it->second.CanBeAcquired(type);
}

PartitionedLockManager::TestLockResult PartitionedLockManager::TestLock(
    PartitionedLockRequest request) {
  // Check if the lock is already taken.
  LockState& lock = locks_[request.lock_id];
  if (!lock.CanBeAcquired(request.type)) {
    return TestLockResult::kLocked;
  }

  // Check if a request is queued up to take the lock in question.
  for (const AcquisitionRequest& queued_request : request_queue_) {
    if (RequestsAreOverlapping({request}, queued_request.lock_requests)) {
      return TestLockResult::kLocked;
    }
  }

  // Otherwise, must be free.
  return TestLockResult::kFree;
}

std::vector<PartitionedLockId> PartitionedLockManager::GetUnacquirableLocks(
    std::vector<PartitionedLockRequest>& lock_requests) {
  std::vector<PartitionedLockId> lock_ids;
  for (PartitionedLockRequest& request : lock_requests) {
    if (TestLock(request) == TestLockResult::kLocked) {
      lock_ids.push_back(request.lock_id);
    }
  }
  return lock_ids;
}

void PartitionedLockManager::AcquireLock(PartitionedLockRequest request,
                                         PartitionedLockHolder& locks_holder) {
  auto it = locks_.find(request.lock_id);
  if (it == locks_.end()) {
    it = locks_
             .emplace(std::piecewise_construct,
                      std::forward_as_tuple(request.lock_id),
                      std::forward_as_tuple())
             .first;
  }

  LockState& state = it->second;
  DCHECK(state.CanBeAcquired(request.type));

  state.access_mode = request.type;
  ++state.acquired_count;
  auto released_callback = base::BindOnce(&PartitionedLockManager::LockReleased,
                                          weak_factory_.GetWeakPtr());
  locks_holder.locks.emplace_back(std::move(request.lock_id),
                                  std::move(released_callback));
}

void PartitionedLockManager::LockReleased(PartitionedLockId released_lock_id) {
  auto it = locks_.find(released_lock_id);
  CHECK(it != locks_.end());
  LockState& state = it->second;
  if (--state.acquired_count > 0) {
    return;
  }

  for (auto iter = request_queue_.begin(); iter != request_queue_.end();) {
    bool exclusive = false;
    if (!base::ranges::any_of(
            iter->lock_requests, [&released_lock_id, &exclusive](
                                     PartitionedLockRequest& lock_request) {
              // We found an interesting lock in the given request. Is the
              // request exclusive?
              if (lock_request.lock_id == released_lock_id) {
                exclusive =
                    exclusive || (lock_request.type == LockType::kExclusive);
                return true;
              }
              return false;
            })) {
      ++iter;
      continue;
    }

    iter = MaybeGrantLocksAndIterate(iter);
    // As an optimization: we can stop if the lock ran into an exclusive
    // request, as no further requests will be grantable.
    if (exclusive) {
      break;
    }
  }
}

void PartitionedLockManager::LockRequestCancelled() {
  auto remove_iter = base::ranges::remove_if(
      request_queue_,
      [](AcquisitionRequest& request) { return !request.locks_holder; });
  if (remove_iter == request_queue_.end()) {
    return;
  }
  // Iterate through the entire queue starting from the erased element, as any
  // subsequent queued request could now be free to start.
  for (auto iter = request_queue_.erase(remove_iter, request_queue_.end());
       iter != request_queue_.end(); MaybeGrantLocksAndIterate(iter)) {
  }
}

std::set<PartitionedLockHolder*> PartitionedLockManager::GetBlockedRequests(
    const base::flat_set<PartitionedLockId>& held_locks) const {
  std::set<PartitionedLockHolder*> blocked_requests;

  // Rebuild the set of lock requests so we can apply
  // `RequestsAreOverlapping()`.
  base::flat_set<PartitionedLockRequest> reconstructed_requests;
  for (const PartitionedLockId& lock_id : held_locks) {
    auto it = locks_.find(lock_id);
    DCHECK(it != locks_.end());
    reconstructed_requests.emplace(lock_id, it->second.access_mode);
  }

  for (const AcquisitionRequest& request : request_queue_) {
    if (RequestsAreOverlapping(request.lock_requests, reconstructed_requests)) {
      blocked_requests.insert(request.locks_holder.get());
    }
  }
  return blocked_requests;
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

}  // namespace content::indexed_db
