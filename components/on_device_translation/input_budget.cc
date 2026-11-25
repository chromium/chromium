// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/input_budget.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace on_device_translation {

InputBudget::Reservation::Reservation(scoped_refptr<InputBudget> budget,
                                      size_t bytes)
    : budget_(std::move(budget)), bytes_(bytes) {}

InputBudget::Reservation::Reservation(Reservation&& other) noexcept
    : budget_(std::move(other.budget_)),
      bytes_(std::exchange(other.bytes_, 0)) {}

InputBudget::Reservation& InputBudget::Reservation::operator=(
    Reservation&& other) noexcept {
  if (this != &other) {
    if (budget_) {
      budget_->GiveBack(bytes_);
    }
    budget_ = std::move(other.budget_);
    bytes_ = std::exchange(other.bytes_, 0);
  }
  return *this;
}

InputBudget::Reservation::~Reservation() {
  if (budget_) {
    budget_->GiveBack(bytes_);
  }
}

InputBudget::Waiter::Waiter(size_t bytes, ReserveCallback callback)
    : bytes(bytes), callback(std::move(callback)) {}
InputBudget::Waiter::Waiter(Waiter&&) = default;
InputBudget::Waiter& InputBudget::Waiter::operator=(Waiter&&) = default;
InputBudget::Waiter::~Waiter() = default;

InputBudget::InputBudget(size_t max_bytes,
                         size_t max_queued_bytes,
                         size_t min_charge_bytes,
                         size_t max_waiters)
    : max_bytes_(max_bytes),
      max_queued_bytes_(max_queued_bytes),
      min_charge_bytes_(min_charge_bytes),
      max_waiters_(max_waiters) {}

InputBudget::~InputBudget() {
  // Each reservation holds a reference.
  CHECK_EQ(reserved_bytes_, 0u);
}

void InputBudget::Reserve(size_t bytes, ReserveCallback callback) {
  const size_t charge = std::max(bytes, min_charge_bytes_);
  while (!waiters_.empty() && waiters_.front().callback.IsCancelled()) {
    CHECK_LE(waiters_.front().bytes, queued_bytes_);
    queued_bytes_ -= waiters_.front().bytes;
    waiters_.pop_front();
  }
  if (waiters_.empty() && Fits(charge)) {
    reserved_bytes_ += charge;
    std::move(callback).Run(Reservation(base::WrapRefCounted(this), charge));
    return;
  }
  if (waiters_.size() >= max_waiters_ ||
      charge > max_queued_bytes_ - queued_bytes_) {
    PruneCancelledWaiters();
    if (waiters_.size() >= max_waiters_ ||
        charge > max_queued_bytes_ - queued_bytes_) {
      std::move(callback).Run(std::nullopt);
      return;
    }
  }
  queued_bytes_ += charge;
  waiters_.emplace_back(charge, std::move(callback));
}

bool InputBudget::Fits(size_t bytes) const {
  return reserved_bytes_ == 0 || (reserved_bytes_ < max_bytes_ &&
                                  bytes <= max_bytes_ - reserved_bytes_);
}

void InputBudget::PruneCancelledWaiters() {
  base::EraseIf(waiters_, [](const Waiter& waiter) {
    return waiter.callback.IsCancelled();
  });
  queued_bytes_ = 0;
  for (const Waiter& waiter : waiters_) {
    queued_bytes_ += waiter.bytes;
  }
}

void InputBudget::GiveBack(size_t bytes) {
  CHECK_LE(bytes, reserved_bytes_);
  reserved_bytes_ -= bytes;
  if (waiters_.empty() || run_waiters_posted_) {
    return;
  }
  // Run the waiters in a new task: this can run inside a reply callback or
  // while a translator goes away.
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  run_waiters_posted_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&InputBudget::RunWaiters, weak_ptr_factory_.GetWeakPtr()));
}

void InputBudget::RunWaiters() {
  run_waiters_posted_ = false;
  // A callback can drop the last other reference to `this`.
  scoped_refptr<InputBudget> self(this);
  while (!waiters_.empty()) {
    Waiter& waiter = waiters_.front();
    if (waiter.callback.IsCancelled()) {
      queued_bytes_ -= waiter.bytes;
      waiters_.pop_front();
      continue;
    }
    if (!Fits(waiter.bytes)) {
      return;
    }
    const size_t bytes = waiter.bytes;
    ReserveCallback callback = std::move(waiter.callback);
    waiters_.pop_front();
    queued_bytes_ -= bytes;
    reserved_bytes_ += bytes;
    std::move(callback).Run(Reservation(self, bytes));
  }
}

}  // namespace on_device_translation
