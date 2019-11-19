// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/scheduled_executor_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/components/nearby/atomic_boolean_impl.h"

namespace chromeos {

namespace nearby {

namespace {

class CancelableTask : public location::nearby::Cancelable {
 public:
  explicit CancelableTask(base::OnceCallback<bool()> cancel_callback)
      : cancel_callback_(std::move(cancel_callback)) {}
  CancelableTask() = default;
  ~CancelableTask() override = default;

  // location::nearby::Cancelable:
  bool cancel() override {
    if (cancel_callback_.is_null())
      return false;

    return std::move(cancel_callback_).Run();
  }

 private:
  base::OnceCallback<bool()> cancel_callback_;
};

}  // namespace

ScheduledExecutorImpl::PendingTaskWithTimer::PendingTaskWithTimer(
    std::shared_ptr<location::nearby::Runnable> runnable,
    std::unique_ptr<base::OneShotTimer> timer)
    : runnable(runnable), timer(std::move(timer)) {}

ScheduledExecutorImpl::PendingTaskWithTimer::~PendingTaskWithTimer() = default;

ScheduledExecutorImpl::ScheduledExecutorImpl(
    scoped_refptr<base::SequencedTaskRunner> timer_task_runner)
    : timer_task_runner_(timer_task_runner),
      is_shut_down_(std::make_unique<AtomicBooleanImpl>()) {
  DETACH_FROM_SEQUENCE(timer_sequence_checker_);
}

ScheduledExecutorImpl::~ScheduledExecutorImpl() = default;

bool ScheduledExecutorImpl::TryCancelTask(
    base::WeakPtr<ScheduledExecutorImpl> executor,
    const base::UnguessableToken& id) {
  if (!executor)
    return false;

  return executor->OnTaskCancelled(id);
}

void ScheduledExecutorImpl::shutdown() {
  if (!is_shut_down_->get())
    is_shut_down_->set(true);
}

std::shared_ptr<location::nearby::Cancelable> ScheduledExecutorImpl::schedule(
    std::shared_ptr<location::nearby::Runnable> runnable,
    int64_t delay_millis) {
  if (is_shut_down_->get())
    return std::make_shared<CancelableTask>();

  auto delayed_task = std::make_unique<PendingTaskWithTimer>(
      runnable, std::make_unique<base::OneShotTimer>());
  delayed_task->timer->SetTaskRunner(timer_task_runner_);

  base::UnguessableToken id = base::UnguessableToken::Create();
  {
    base::AutoLock al(map_lock_);
    id_to_task_map_[id] = std::move(delayed_task);
  }

  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ScheduledExecutorImpl::StartTimerWithId,
                     base::Unretained(this), id,
                     base::TimeDelta::FromMilliseconds(delay_millis)));

  return std::make_shared<CancelableTask>(
      base::BindOnce(&TryCancelTask, weak_factory_.GetWeakPtr(), id));
}

void ScheduledExecutorImpl::StartTimerWithId(const base::UnguessableToken& id,
                                             const base::TimeDelta& delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(timer_sequence_checker_);
  base::AutoLock al(map_lock_);

  // If the id no longer exists, it means the task has already been cancelled.
  auto it = id_to_task_map_.find(id);
  if (it == id_to_task_map_.end())
    return;

  it->second->timer->Start(FROM_HERE, delay,
                           base::BindOnce(&ScheduledExecutorImpl::RunTaskWithId,
                                          base::Unretained(this), id));
}

void ScheduledExecutorImpl::StopTimerWithIdAndDeleteTaskEntry(
    const base::UnguessableToken& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(timer_sequence_checker_);
  base::AutoLock al(map_lock_);

  // If the id no longer exists, it means the task has either already been run,
  // or the task has already been cancelled.
  auto it = id_to_task_map_.find(id);
  if (it == id_to_task_map_.end())
    return;

  it->second->timer->Stop();
  id_to_task_map_.erase(id);
}

void ScheduledExecutorImpl::RunTaskWithId(const base::UnguessableToken& id) {
  base::AutoLock al(map_lock_);

  auto it = id_to_task_map_.find(id);
  if (it == id_to_task_map_.end())
    return;

  it->second->runnable->run();
  id_to_task_map_.erase(id);
}

bool ScheduledExecutorImpl::OnTaskCancelled(const base::UnguessableToken& id) {
  {
    base::AutoLock al(map_lock_);
    auto it = id_to_task_map_.find(id);
    if (it == id_to_task_map_.end())
      return false;
  }

  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ScheduledExecutorImpl::StopTimerWithIdAndDeleteTaskEntry,
                     base::Unretained(this), id));
  return true;
}

}  // namespace nearby

}  // namespace chromeos
