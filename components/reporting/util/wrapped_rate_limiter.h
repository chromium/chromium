// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_WRAPPED_RATE_LIMITER_H_
#define COMPONENTS_REPORTING_UTIL_WRAPPED_RATE_LIMITER_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/util/rate_limiter_interface.h"

namespace reporting {

// This class wraps around rate limiter instance to ensure the latter
// is used sequentially. It is instantiated by calling
// `WrappedRateLimiter::Create`.
//
// The usage snippet in an owner class:
//
// class RateLimitedEventSender {
//  public:
//   explicit RateLimitedEventSender(
//       std::unique_ptr<RateLimiterInterface> rate_limiter)
//       : wrapped_rate_limiter_(
//             WrappedRateLimiter::Create(std::move(rate_limiter))),
//         async_acquire_cb_(wrapped_rate_limiter_->async_acquire_cb()) {}
//
//   void SendEvent(Event event, EnqueueCallback cb) {
//     const size_t event_size = event.ByteSizeLong();
//     async_acquire_cb_.Run(
//         event_size,
//         base::BindOnce([](Event event, EnqueueCallback cb, bool acquired) {
//             if (!acquired) {
//               std::move(cb).Run(Status{error::OUT_OF_RANGE, "..."});
//               return;
//             }
//             ...  // Actually enqueue `event`, using `cb` to return outcome.
//         }, std::move(event), std::move(cb));
//   }
//
//  private:
//   WrappedRateLimiter::SmartPtr wrapped_rate_limiter_;
//   AsyncAcquireCb async_acquire_cb_;
// };
//
class WrappedRateLimiter {
 public:
  using SmartPtr =
      std::unique_ptr<WrappedRateLimiter, base::OnTaskRunnerDeleter>;

  using AsyncAcquireCb =
      base::RepeatingCallback<void(size_t /*event_size*/,
                                   base::OnceCallback<void(bool)> /*cb*/)>;

  // Events rate limiting UMA metric name.
  static constexpr char kRateLimitedEventsUma[] =
      "Browser.ERP.RateLimitedEvents";

  // Creates wrapped rate limiter that ensures sequenced access to `Acquire`.
  static SmartPtr Create(std::unique_ptr<RateLimiterInterface> rate_limiter);

  WrappedRateLimiter(const WrappedRateLimiter&) = delete;
  WrappedRateLimiter& operator=(const WrappedRateLimiter&) = delete;
  ~WrappedRateLimiter();

  // Returns `AsyncAcquireCb` callback that invokes `Acquire` on
  // `sequenced_task_runner_` and then passes the result to `cb` on the current
  // task runner.
  AsyncAcquireCb async_acquire_cb() const;

 private:
  // Constructor can only be called by `Create` factory method above.
  WrappedRateLimiter(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
      std::unique_ptr<RateLimiterInterface> rate_limiter);

  // Helper method, forwards `Acquire` call to owned rate limiter, if wrapper is
  // still valid, otherwise returns `false`. Must be called on
  // `sequence_task_runner_`.
  static bool Acquire(base::WeakPtr<WrappedRateLimiter> self,
                      size_t event_size);

  // Helper method, calls `Acquire` on `sequenced_task_runner_` and then passes
  // the result to `cb` on the current task runner, if wrapper is still valid,
  // otherwise calls `cb` with `false`.
  static void AsyncAcquire(base::WeakPtr<WrappedRateLimiter> self,
                           size_t event_size,
                           base::OnceCallback<void(bool)> cb);

  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Wrapped rate limiter instance.
  const std::unique_ptr<RateLimiterInterface> rate_limiter_;

  // AsyncAcquireCb callback. Set once only, by constructor.
  AsyncAcquireCb async_acquire_cb_;

  // Weak ptr factory.
  base::WeakPtrFactory<WrappedRateLimiter> weak_ptr_factory_{this};
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_WRAPPED_RATE_LIMITER_H_
