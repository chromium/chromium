// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/resources/resource_manager.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <utility>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace reporting {

ResourceManager::ResourceManager(uint64_t total_size)
    : total_(total_size),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT})) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ResourceManager::~ResourceManager() = default;

bool ResourceManager::Reserve(uint64_t size) {
  uint64_t old_used = used_.fetch_add(size);
  if (old_used + size > total_) {
    used_.fetch_sub(size);
    return false;
  }
  return true;
}

void ResourceManager::Discard(uint64_t size) {
  CHECK_LE(size, used_.load());
  used_.fetch_sub(size);

  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ResourceManager::FlushCallbacks,
                                base::WrapRefCounted(this)));
}

uint64_t ResourceManager::GetTotal() const {
  return total_;
}

uint64_t ResourceManager::GetUsed() const {
  return used_.load();
}

void ResourceManager::Test_SetTotal(uint64_t test_total) {
  total_ = test_total;
}

void ResourceManager::RegisterCallback(uint64_t size, base::OnceClosure cb) {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](scoped_refptr<ResourceManager> self, uint64_t size,
                        base::OnceClosure cb) {
                       DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
                       self->resource_callbacks_.emplace(size, std::move(cb));

                       // Attempt to apply remaining callbacks
                       // (this is especially important if the new callback is
                       // registered with no allocations to be released - we
                       // don't want the callback to wait indefinitely in this
                       // case).
                       self->FlushCallbacks();
                     },
                     base::WrapRefCounted(this), size,
                     base::BindPostTaskToCurrentDefault(std::move(cb))));
}

void ResourceManager::FlushCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t remained = GetTotal() - GetUsed();  // No synchronization whatsoever.
  while (remained > 0 && !resource_callbacks_.empty()) {
    auto& cb_pair = resource_callbacks_.front();
    if (cb_pair.first > static_cast<uint64_t>(remained)) {
      break;
    }
    remained -= cb_pair.first;
    std::move(cb_pair.second).Run();
    resource_callbacks_.pop();
  }
}

ScopedReservation::ScopedReservation() noexcept = default;

ScopedReservation::ScopedReservation(
    uint64_t size,
    scoped_refptr<ResourceManager> resource_manager) noexcept
    : resource_manager_(resource_manager) {
  if (size == 0uL || !resource_manager->Reserve(size)) {
    return;
  }
  size_ = size;
}

ScopedReservation::ScopedReservation(
    uint64_t size,
    const ScopedReservation& other_reservation) noexcept
    : resource_manager_(other_reservation.resource_manager_) {
  if (size == 0uL || !resource_manager_.get() ||
      !resource_manager_->Reserve(size)) {
    return;
  }
  size_ = size;
}

ScopedReservation::ScopedReservation(ScopedReservation&& other) noexcept
    : resource_manager_(other.resource_manager_),
      size_(std::exchange(other.size_, std::nullopt)) {}

bool ScopedReservation::reserved() const {
  return size_.has_value();
}

bool ScopedReservation::Reduce(uint64_t new_size) {
  if (!reserved()) {
    return false;
  }
  if (new_size < 0 || size_.value() < new_size) {
    return false;
  }
  resource_manager_->Discard(size_.value() - new_size);
  if (new_size > 0) {
    size_ = new_size;
  } else {
    size_ = std::nullopt;
  }
  return true;
}

void ScopedReservation::HandOver(ScopedReservation& other) {
  if (resource_manager_.get()) {
    CHECK_EQ(resource_manager_.get(), other.resource_manager_.get())
        << "Reservations are not related";
  } else {
    CHECK(!reserved()) << "Unattached reservation may not have size";
    resource_manager_ = other.resource_manager_;
  }
  if (!other.reserved()) {
    return;  // Nothing changes.
  }
  const uint64_t old_size = (reserved() ? size_.value() : 0uL);
  size_ = old_size + std::exchange(other.size_, std::nullopt).value();
}

ScopedReservation::~ScopedReservation() {
  if (reserved()) {
    resource_manager_->Discard(size_.value());
  }
}
}  // namespace reporting
