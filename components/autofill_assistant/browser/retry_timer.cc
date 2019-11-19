// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/retry_timer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

RetryTimer::RetryTimer(base::TimeDelta period) : period_(period) {}
RetryTimer::~RetryTimer() = default;

void RetryTimer::Start(
    base::TimeDelta max_wait_time,
    base::RepeatingCallback<void(base::OnceCallback<void(const ClientStatus&)>)>
        task,
    base::OnceCallback<void(const ClientStatus&)> on_done) {
  Reset();
  task_ = std::move(task);
  on_done_ = std::move(on_done);
  if (max_wait_time <= base::TimeDelta::FromSeconds(0)) {
    remaining_attempts_ = 1;
  } else {
    remaining_attempts_ = 1 + max_wait_time / period_;
  }
  DCHECK_GE(remaining_attempts_, 1);
  RunTask();
}

void RetryTimer::Cancel() {
  if (!on_done_)
    return;

  Reset();
}

void RetryTimer::Reset() {
  timer_.reset();
  task_id_++;  // cancels any pending OnTaskDone callbacks
  task_.Reset();
  on_done_.Reset();
}

void RetryTimer::RunTask() {
  task_.Run(base::BindOnce(&RetryTimer::OnTaskDone,
                           weak_ptr_factory_.GetWeakPtr(), task_id_));
}

void RetryTimer::OnTaskDone(int64_t task_id, const ClientStatus& status) {
  if (task_id != task_id_)  // Ignore callbacks from cancelled tasks
    return;

  remaining_attempts_--;
  if (status.ok() || remaining_attempts_ <= 0) {
    CHECK_GE(remaining_attempts_, 0);
    task_.Reset();  // release any resources held by the callback
    std::move(on_done_).Run(status);
    // Don't do anything after calling on_done_, as it could have deleted this.
    return;
  }
  timer_ = std::make_unique<base::OneShotTimer>();
  timer_->Start(FROM_HERE, period_,
                base::BindOnce(&RetryTimer::RunTask,
                               // Safe, since timer_ is owned by this instance
                               base::Unretained(this)));
}

}  // namespace autofill_assistant
