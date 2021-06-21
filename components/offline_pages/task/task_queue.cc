// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/task_queue.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace offline_pages {

struct TaskQueue::Entry {
  Entry() = default;
  explicit Entry(std::unique_ptr<Task> task) : task(std::move(task)) {}
  Entry(std::unique_ptr<Task> task, base::OnceClosure resume_callback)
      : task(std::move(task)), resume_callback(std::move(resume_callback)) {}

  std::unique_ptr<Task> task;
  base::OnceClosure resume_callback;
};

TaskQueue::TaskQueue(Delegate* delegate)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()), delegate_(delegate) {
  DCHECK(delegate_);
}

TaskQueue::~TaskQueue() {}

void TaskQueue::AddTask(std::unique_ptr<Task> task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task->task_queue_ = this;
  tasks_.emplace_back(std::move(task));
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

  current_task_ = std::move(tasks_.front().task);
  base::OnceClosure resume_callback = std::move(tasks_.front().resume_callback);
  tasks_.pop_front();
  if (resume_callback) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&TaskQueue::ResumeCurrentTask,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          std::move(resume_callback)));
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&TaskQueue::RunCurrentTask,
                                          weak_ptr_factory_.GetWeakPtr()));
  }
}

void TaskQueue::RunCurrentTask() {
  current_task_->Execute(base::BindOnce(&TaskCompletedCallback, task_runner_,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        current_task_.get()));
}

void TaskQueue::ResumeCurrentTask(base::OnceClosure on_resume) {
  DCHECK_EQ(Task::TaskState::kSuspended, current_task_->state_);
  current_task_->state_ = Task::TaskState::kRunning;
  std::move(on_resume).Run();
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
  // Normally, the completed task is the current task.
  if (task == current_task_.get()) {
    current_task_.reset(nullptr);
    StartTaskIfAvailable();
    return;
  }

  // If the task is in the suspended_tasks_ list, remove it.
  for (auto iter = suspended_tasks_.begin(); iter != suspended_tasks_.end();
       ++iter) {
    if (iter->get() == task) {
      suspended_tasks_.erase(iter);
      return;
    }
  }

  // Otherwise, this is an enqueued task. Find and remove it.
  for (auto iter = tasks_.begin(); iter != tasks_.end(); ++iter) {
    if (iter->task.get() == task) {
      tasks_.erase(iter);
      return;
    }
  }

  NOTREACHED() << "TaskCompleted: cannot find task";
}

void TaskQueue::SuspendTask(Task* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Task::Suspend() sets state to kSuspended.
  DCHECK_EQ(Task::TaskState::kSuspended, task->state_);
  DCHECK_EQ(task, current_task_.get());
  suspended_tasks_.push_back(std::move(current_task_));
  StartTaskIfAvailable();
}

void TaskQueue::ResumeTask(Task* task, base::OnceClosure on_resume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(Task::TaskState::kSuspended, task->state_);
  for (auto iter = suspended_tasks_.begin(); iter != suspended_tasks_.end();
       ++iter) {
    if (iter->get() == task) {
      tasks_.emplace_back(std::move(*iter), std::move(on_resume));
      suspended_tasks_.erase(iter);
      StartTaskIfAvailable();
      return;
    }
  }

  NOTREACHED() << "Trying to resume task that's not suspended";
}

void TaskQueue::InformTaskQueueIsIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnTaskQueueIsIdle();
}

}  // namespace offline_pages
