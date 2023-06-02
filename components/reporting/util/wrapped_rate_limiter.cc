// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/wrapped_rate_limiter.h"

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "components/reporting/util/rate_limiter_interface.h"

namespace reporting {

// static
WrappedRateLimiter::SmartPtr WrappedRateLimiter::Create(
    std::unique_ptr<RateLimiterInterface> rate_limiter) {
  auto sequenced_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT});
  return SmartPtr(
      new WrappedRateLimiter(sequenced_task_runner, std::move(rate_limiter)),
      base::OnTaskRunnerDeleter(sequenced_task_runner));
}

WrappedRateLimiter::~WrappedRateLimiter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
bool WrappedRateLimiter::Acquire(base::WeakPtr<WrappedRateLimiter> self,
                                 size_t event_size) {
  if (!self) {
    base::UmaHistogramBoolean(kRateLimitedEventsUma, false);
    return false;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
  if (!self->rate_limiter_->Acquire(event_size)) {
    base::UmaHistogramBoolean(kRateLimitedEventsUma, false);
    return false;
  }
  base::UmaHistogramBoolean(kRateLimitedEventsUma, true);
  return true;
}

// static
void WrappedRateLimiter::AsyncAcquire(base::WeakPtr<WrappedRateLimiter> self,
                                      size_t event_size,
                                      base::OnceCallback<void(bool)> cb) {
  base::BindOnce(&WrappedRateLimiter::Acquire, self)
      .Then(std::move(cb))
      .Run(event_size);
}

WrappedRateLimiter::AsyncAcquireCb WrappedRateLimiter::async_acquire_cb()
    const {
  return async_acquire_cb_;
}

WrappedRateLimiter::WrappedRateLimiter(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    std::unique_ptr<RateLimiterInterface> rate_limiter)
    : sequenced_task_runner_(sequenced_task_runner),
      rate_limiter_(std::move(rate_limiter)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  async_acquire_cb_ =
      base::BindPostTask(sequenced_task_runner_,
                         base::BindRepeating(&WrappedRateLimiter::AsyncAcquire,
                                             weak_ptr_factory_.GetWeakPtr()));
}
}  // namespace reporting
