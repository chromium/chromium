// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/scheduled_executor.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/services/sharing/nearby/platform/atomic_boolean.h"

namespace nearby::chrome {

namespace {

class CancelableTask : public api::Cancelable {
 public:
  explicit CancelableTask(base::OnceCallback<bool()> cancel_callback)
      : cancel_callback_(std::move(cancel_callback)) {}
  CancelableTask() = default;
  ~CancelableTask() override = default;

  // api::Cancelable:
  bool Cancel() override {
    if (cancel_callback_.is_null())
      return false;

    return std::move(cancel_callback_).Run();
  }

 private:
  base::OnceCallback<bool()> cancel_callback_;
};

}  // namespace

ScheduledExecutor::PendingTaskWithTimer::PendingTaskWithTimer(
    Runnable&& runnable)
    : runnable(std::move(runnable)) {}

ScheduledExecutor::PendingTaskWithTimer::~PendingTaskWithTimer() = default;

ScheduledExecutor::ScheduledExecutor(
    scoped_refptr<base::SequencedTaskRunner> timer_task_runner)
    : timer_task_runner_(std::move(timer_task_runner)) {
  DETACH_FROM_SEQUENCE(timer_sequence_checker_);
}

ScheduledExecutor::~ScheduledExecutor() {
  // Move all runnables from id_to_task_map_ to pending_tasks to avoid blocking
  // Schedule or Cancel while executing runnables.
  std::map<base::UnguessableToken, std::unique_ptr<PendingTaskWithTimer>>
      pending_tasks;
  {
    base::AutoLock al(lock_);
    is_shut_down_ = true;
    using std::swap;
    swap(pending_tasks, id_to_task_map_);
  }

  // Run all tasks prematurely, order does not matter.
  {
    // base::ScopedAllowBaseSyncPrimitives is required as code inside the
    // runnable uses blocking primitive, which lives outside Chrome.
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    for (auto& it : pending_tasks)
      it.second->runnable();
  }
}

bool ScheduledExecutor::TryCancelTask(base::WeakPtr<ScheduledExecutor> executor,
                                      const base::UnguessableToken& id) {
  if (!executor)
    return false;

  return executor->OnTaskCancelled(id);
}

void ScheduledExecutor::Execute(Runnable&& runnable) {
  Schedule(std::move(runnable), absl::ZeroDuration());
}

void ScheduledExecutor::Shutdown() {
  base::AutoLock al(lock_);
  is_shut_down_ = true;
}

std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(
    Runnable&& runnable,
    absl::Duration duration) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  {
    base::AutoLock al(lock_);
    if (is_shut_down_)
      return std::make_shared<CancelableTask>();

    id_to_task_map_.emplace(
        id, std::make_unique<PendingTaskWithTimer>(std::move(runnable)));
  }

  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ScheduledExecutor::StartTimerWithId,
                     timer_task_runner_weak_factory_.GetWeakPtr(), id,
                     base::Microseconds(absl::ToInt64Microseconds(duration))));

  return std::make_shared<CancelableTask>(base::BindOnce(
      &TryCancelTask, cancelable_task_weak_factory_.GetWeakPtr(), id));
}

void ScheduledExecutor::StartTimerWithId(const base::UnguessableToken& id,
                                         base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(timer_sequence_checker_);
  base::AutoLock al(lock_);

  // If the id no longer exists, it means the task has already been cancelled.
  auto it = id_to_task_map_.find(id);
  if (it == id_to_task_map_.end())
    return;

  it->second->timer.SetTaskRunner(timer_task_runner_);
  it->second->timer.Start(
      FROM_HERE, delay,
      base::BindOnce(&ScheduledExecutor::RunTaskWithId,
                     timer_task_runner_weak_factory_.GetWeakPtr(), id));
}

void ScheduledExecutor::StopTimerWithIdAndDeleteTaskEntry(
    const base::UnguessableToken& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(timer_sequence_checker_);
  base::AutoLock al(lock_);

  // If the id no longer exists, it means the task has either already been run,
  // or the task has already been cancelled.
  auto it = id_to_task_map_.find(id);
  if (it == id_to_task_map_.end())
    return;

  it->second->timer.Stop();
  id_to_task_map_.erase(id);
}

void ScheduledExecutor::RunTaskWithId(const base::UnguessableToken& id) {
  Runnable runnable;
  {
    base::AutoLock al(lock_);

    auto it = id_to_task_map_.find(id);
    if (it == id_to_task_map_.end())
      return;

    runnable = std::move(it->second->runnable);
    id_to_task_map_.erase(id);
  }

  {
    // base::ScopedAllowBaseSyncPrimitives is required as code inside the
    // runnable uses blocking primitive, which lives outside Chrome.
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    runnable();
  }
}

bool ScheduledExecutor::OnTaskCancelled(const base::UnguessableToken& id) {
  {
    base::AutoLock al(lock_);
    auto it = id_to_task_map_.find(id);
    if (it == id_to_task_map_.end())
      return false;
  }

  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ScheduledExecutor::StopTimerWithIdAndDeleteTaskEntry,
                     timer_task_runner_weak_factory_.GetWeakPtr(), id));
  return true;
}

}  // namespace nearby::chrome
