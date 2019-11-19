// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/task_queue.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace offline_pages {

TaskQueue::TaskQueue(Delegate* delegate)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()), delegate_(delegate) {
  DCHECK(delegate_);
}

TaskQueue::~TaskQueue() {}

void TaskQueue::AddTask(std::unique_ptr<Task> task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task->SetTaskCompletionCallback(base::BindOnce(
      &TaskCompletedCallback, task_runner_, weak_ptr_factory_.GetWeakPtr()));
  tasks_.push(std::move(task));
  StartTaskIfAvailable();
}

bool TaskQueue::HasPendingTasks() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !tasks_.empty() || HasRunningTask();
}

bool TaskQueue::HasRunningTask() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_task_ != nullptr;
}

void TaskQueue::StartTaskIfAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "running? " << HasRunningTask() << ", pending? "
           << HasPendingTasks() << " " << __func__;
  if (HasRunningTask())
    return;

  if (!HasPendingTasks()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&TaskQueue::InformTaskQueueIsIdle,
                                          weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  current_task_ = std::move(tasks_.front());
  tasks_.pop();
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TaskQueue::RunCurrentTask,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void TaskQueue::RunCurrentTask() {
  current_task_->Run();
}

// static
void TaskQueue::TaskCompletedCallback(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<TaskQueue> task_queue,
    Task* task) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&TaskQueue::TaskCompleted, task_queue, task));
}

void TaskQueue::TaskCompleted(Task* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(task, current_task_.get());
  if (task == current_task_.get()) {
    current_task_.reset(nullptr);
    StartTaskIfAvailable();
  }
}

void TaskQueue::InformTaskQueueIsIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnTaskQueueIsIdle();
}

}  // namespace offline_pages
