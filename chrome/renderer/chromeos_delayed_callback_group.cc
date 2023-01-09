// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chromeos_delayed_callback_group.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

DelayedCallbackGroup::CallbackEntry::CallbackEntry(
    Callback callback,
    const scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::TimeTicks expiration_time)
    : callback_(std::move(callback)),
      callback_task_runner_(std::move(callback_task_runner)),
      expiration_time_(expiration_time) {}

DelayedCallbackGroup::CallbackEntry::~CallbackEntry() {}

DelayedCallbackGroup::DelayedCallbackGroup(
    base::TimeDelta expiration_delay,
    scoped_refptr<base::SequencedTaskRunner> expiration_task_runner)
    : expiration_delay_(expiration_delay),
      expiration_task_runner_(expiration_task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DelayedCallbackGroup::~DelayedCallbackGroup() {
  base::AutoLock lock(callbacks_lock_);
  CancelExpirationTimer();
  ExpireAllCallbacks();
}

void DelayedCallbackGroup::Add(Callback callback) {
  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());
  {
    base::AutoLock lock(callbacks_lock_);
    base::TimeTicks expiration_time =
        base::TimeTicks::Now() + expiration_delay_;
    callbacks_.emplace(std::move(callback),
                       base::SequencedTaskRunner::GetCurrentDefault(),
                       expiration_time);
  }
  expiration_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DelayedCallbackGroup::StartExpirationTimer, this));
}

void DelayedCallbackGroup::CancelExpirationTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  expiration_timeout_.Cancel();
}

void DelayedCallbackGroup::StartExpirationTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(callbacks_lock_);
  if (callbacks_.empty() || !expiration_timeout_.IsCancelled())
    return;

  base::TimeDelta delay_until_next_expiration =
      callbacks_.front().expiration_time_ - base::TimeTicks::Now();
  expiration_timeout_.Reset(
      base::BindOnce(&DelayedCallbackGroup::OnExpirationTimer, this));
  expiration_task_runner_->PostDelayedTask(
      FROM_HERE, expiration_timeout_.callback(), delay_until_next_expiration);
}

void DelayedCallbackGroup::OnExpirationTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ProcessExpiredCallbacks(base::TimeTicks::Now());
  StartExpirationTimer();
}

void DelayedCallbackGroup::RunAll() {
  base::AutoLock lock(callbacks_lock_);
  while (!callbacks_.empty()) {
    CallbackEntry& entry = callbacks_.front();
    entry.callback_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(entry.callback_), RunReason::NORMAL));
    callbacks_.pop();
  }
}

void DelayedCallbackGroup::ProcessExpiredCallbacks(
    base::TimeTicks expiration_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock lock(callbacks_lock_);
  CancelExpirationTimer();
  while (!callbacks_.empty()) {
    CallbackEntry& entry = callbacks_.front();
    if (entry.expiration_time_ <= expiration_time) {
      entry.callback_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(entry.callback_), RunReason::TIMEOUT));
      callbacks_.pop();
    } else {
      // All others in this queue expire after |expiration_time|.
      return;
    }
  }
}

void DelayedCallbackGroup::ExpireAllCallbacks() {
  while (!callbacks_.empty()) {
    CallbackEntry& entry = callbacks_.front();
    entry.callback_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(entry.callback_), RunReason::TIMEOUT));
    callbacks_.pop();
  }
}
