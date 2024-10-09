// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/task_queue.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"

namespace offline_pages {

struct TaskQueue::Entry {
  Entry() = default;
  explicit Entry(std::unique_ptr<Task> task) : task(std::move(task)) {}
  Entry(const base::Location& location, std::unique_ptr<Task> task)
      : task(std::move(task)), from_here(location) {}
  Entry(std::unique_ptr<Task> task, base::OnceClosure resume_callback)
      : task(std::move(task)), resume_callback(std::move(resume_callback)) {}

  std::unique_ptr<Task> task;
  base::OnceClosure resume_callback;
  base::Location from_here;
};

TaskQueue::TaskQueue(Delegate* delegate)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      delegate_(delegate) {
  DCHECK(delegate_);
}

TaskQueue::~TaskQueue() = default;

void TaskQueue::AddTask(std::unique_ptr<Task> task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task->task_queue_ = this;
  tasks_.emplace_back(std::move(task));
  StartTaskIfAvailable();
}

void TaskQueue::AddTask(const base::Location& from_here,
                        std::unique_ptr<Task> task) {
  DVLOG(2) << "Adding task " << from_here.ToString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task->task_queue_ = this;
  tasks_.emplace_back(from_here, std::move(task));
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
  current_task_location_ = tasks_.front().from_here;

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
  DVLOG(2) << "Running task " << current_task_location_.ToString();
  current_task_->Execute(base::BindOnce(&TaskCompletedCallback, task_runner_,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        current_task_.get()));
}

void TaskQueue::ResumeCurrentTask(base::OnceClosure on_resume) {
  DVLOG(2) << "Resuming task " << current_task_location_.ToString();
  DCHECK_EQ(Task::TaskState::kPendingResume, current_task_->state_);
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
    DVLOG(2) << "Current task completed " << current_task_location_.ToString();
    StartTaskIfAvailable();
    return;
  }

  // If the task is in the suspended_tasks_ list, remove it.
  for (auto iter = suspended_tasks_.begin(); iter != suspended_tasks_.end();
       ++iter) {
    if (iter->task.get() == task) {
      DVLOG(2) << "Suspended task completed " << iter->from_here.ToString();
      suspended_tasks_.erase(iter);
      return;
    }
  }

  NOTREACHED_IN_MIGRATION() << "TaskCompleted: cannot find task";
}

void TaskQueue::SuspendTask(Task* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Task::Suspend() sets state to kSuspended.
  DCHECK_EQ(Task::TaskState::kSuspended, task->state_);
  DCHECK_EQ(task, current_task_.get());
  suspended_tasks_.emplace_back(current_task_location_,
                                std::move(current_task_));
  StartTaskIfAvailable();
}

void TaskQueue::ResumeTask(Task* task, base::OnceClosure on_resume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(Task::TaskState::kSuspended, task->state_);
  for (auto iter = suspended_tasks_.begin(); iter != suspended_tasks_.end();
       ++iter) {
    if (iter->task.get() == task) {
      iter->resume_callback = std::move(on_resume);
      tasks_.push_back(std::move(*iter));
      suspended_tasks_.erase(iter);
      task->state_ = Task::TaskState::kPendingResume;
      StartTaskIfAvailable();
      return;
    }
  }

  NOTREACHED_IN_MIGRATION() << "Trying to resume task that's not suspended";
}

void TaskQueue::InformTaskQueueIsIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnTaskQueueIsIdle();
}

// Returns a human-readable string describing the contents of the task queue.
std::string TaskQueue::GetStateForTesting() const {
  std::stringstream ss;
  if (current_task_) {
    ss << "Current task: " << current_task_location_.ToString() << '\n';
  } else {
    ss << "No current task\n";
  }
  int number = 1;
  for (const auto& entry : tasks_) {
    ss << "Pending task " << number++ << ": " << entry.from_here.ToString()
       << '\n';
  }
  number = 1;
  for (const auto& entry : suspended_tasks_) {
    ss << "Suspended task " << number++ << ": " << entry.from_here.ToString()
       << '\n';
  }
  return ss.str();
}

}  // namespace offline_pages
