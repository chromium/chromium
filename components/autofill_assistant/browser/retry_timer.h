// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_RETRY_TIMER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_RETRY_TIMER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

// Run an asynchronous task until either the task succeeds or time runs out.
//
// RetryTimer cancels the timer when it goes out of scope. This makes it easy to
// guarantee that the callbacks are not called once the owning object has gone
// out of scope.
class RetryTimer {
 public:
  // A RetryTimer with the given retry period
  explicit RetryTimer(base::TimeDelta period);
  ~RetryTimer();

  // Tries to run |task| once immediately and then periodically until it
  // either succeeds or times out. Reports the result to |on_done|.
  //
  // |task| must report the result to the callback passed to it. Retries are
  // interrupted until that happens. If |task| never calls its callback, retries
  // might never happen.
  //
  // Calling Start while an attempt is in progress cancels that attempt.
  // |on_done| for the previous attempt will not be called in that case.
  //
  // If |max_wait_time| is 0 or lower than the retry period, the task is
  // executed exactly once.
  void Start(base::TimeDelta max_wait_time,
             base::RepeatingCallback<
                 void(base::OnceCallback<void(const ClientStatus&)>)> task,
             base::OnceCallback<void(const ClientStatus&)> on_done);

  // Cancels any pending tasks or timer. Any |on_done| callbacks passed to Start
  // is released without being called.
  //
  // Does nothing if to tasks are in progress.
  void Cancel();

  // Returns true if the timer was started but did not report any results yet.
  bool running() { return on_done_ ? true : false; }

 private:
  void Reset();
  void RunTask();
  void OnTaskDone(int64_t task_id_, const ClientStatus& status);

  const base::TimeDelta period_;
  int64_t remaining_attempts_ = 1;
  int64_t task_id_ = 0;
  base::RepeatingCallback<void(base::OnceCallback<void(const ClientStatus&)>)>
      task_;
  base::OnceCallback<void(const ClientStatus&)> on_done_;
  std::unique_ptr<base::OneShotTimer> timer_;

  base::WeakPtrFactory<RetryTimer> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(RetryTimer);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_RETRY_TIMER_H_
