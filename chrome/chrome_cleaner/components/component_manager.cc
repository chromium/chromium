// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/components/component_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/chrome_cleaner/components/component_api.h"

namespace chrome_cleaner {

namespace {

// A set of factory functions to create the calls to code to be executed in
// worker threads. Since the code to loop through the components is the same
// for all calls, it was extracted in the |PostComponentTasks| method. So the
// public API methods must specify which function to call on the components.
// Since the component pointer must be the first argument, and we can't pre-pend
// args to bound calls, the public API methods sends a bound version of these
// factory functions so that |PostComponentTasks| can call them to get the
// closure by passing the component pointer to them. The component pointer
// must be the last argument to these factory functions so that the other
// arguments are pre-bound by the public API methods when they call
// |PostComponentTasks| (e.g., |found_pups|).
base::OnceClosure BindPreScan(ComponentAPI* component) {
  return base::BindOnce(&ComponentAPI::PreScan, base::Unretained(component));
}

base::OnceClosure BindPostScan(const std::vector<UwSId>& found_pups,
                               ComponentAPI* component) {
  return base::BindOnce(&ComponentAPI::PostScan, base::Unretained(component),
                        found_pups);
}

base::OnceClosure BindPreCleanup(ComponentAPI* component) {
  return base::BindOnce(&ComponentAPI::PreCleanup, base::Unretained(component));
}

base::OnceClosure BindPostCleanup(ResultCode result_code,
                                  RebooterAPI* rebooter,
                                  ComponentAPI* component) {
  return base::BindOnce(&ComponentAPI::PostCleanup, base::Unretained(component),
                        result_code, rebooter);
}

}  // namespace

ComponentManager::ComponentManager(ComponentManagerDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

ComponentManager::~ComponentManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Make sure |CloseAllComponents| was called. IT can't be called from here
  // because it needs the result code. The components will still be released by
  // the scoped vector, but their OnClose won't be called if
  // |CloseAllComponents| wasn't called.
  DCHECK(!cancelable_task_tracker_.HasTrackedTasks());
  DCHECK(components_.empty());
  DCHECK(done_callback_.is_null());
}

void ComponentManager::AddComponent(std::unique_ptr<ComponentAPI> component) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Components can't be added while tasks are in flight.
  DCHECK(!cancelable_task_tracker_.HasTrackedTasks());
  DCHECK(done_callback_.is_null());
  DCHECK_EQ(0UL, num_tasks_pending_);

  components_.push_back(std::move(component));
}

void ComponentManager::PreScan() {
  // No task should be pending when any method from the ComponentsAPI are
  // called.
  DCHECK(done_callback_.is_null());
  done_callback_ = base::BindOnce(&ComponentManagerDelegate::PreScanDone,
                                  base::Unretained(delegate_));
  PostComponentTasks(base::BindRepeating(&BindPreScan), "PreScan");
}

void ComponentManager::PostScan(const std::vector<UwSId>& found_pups) {
  DCHECK(done_callback_.is_null());
  done_callback_ = base::BindOnce(&ComponentManagerDelegate::PostScanDone,
                                  base::Unretained(delegate_));
  PostComponentTasks(base::BindRepeating(&BindPostScan, found_pups),
                     "PostScan");
}

void ComponentManager::PreCleanup() {
  DCHECK(done_callback_.is_null());
  done_callback_ = base::BindOnce(&ComponentManagerDelegate::PreCleanupDone,
                                  base::Unretained(delegate_));
  PostComponentTasks(base::BindRepeating(&BindPreCleanup), "PreCleanup");
}

void ComponentManager::PostCleanup(ResultCode result_code,
                                   RebooterAPI* rebooter) {
  DCHECK(done_callback_.is_null());
  done_callback_ = base::BindOnce(&ComponentManagerDelegate::PostCleanupDone,
                                  base::Unretained(delegate_));
  PostComponentTasks(
      base::BindRepeating(&BindPostCleanup, result_code, rebooter),
      "PostCleanup");
}

void ComponentManager::PostValidation(ResultCode result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!cancelable_task_tracker_.HasTrackedTasks());
  DCHECK_EQ(0UL, num_tasks_pending_);

  for (auto& component : components_)
    component->PostValidation(result_code);
}

void ComponentManager::CloseAllComponents(ResultCode result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (num_tasks_pending_ > 0) {
    cancelable_task_tracker_.TryCancelAll();
    while (cancelable_task_tracker_.HasTrackedTasks())
      base::RunLoop().RunUntilIdle();
    done_callback_.Reset();
  } else {
    DCHECK(!cancelable_task_tracker_.HasTrackedTasks());
  }

  for (auto& component : components_)
    component->OnClose(result_code);
  components_.clear();
}

void ComponentManager::PostComponentTasks(
    const base::RepeatingCallback<base::OnceClosure(ComponentAPI*)>
        component_task,
    const char* method_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!cancelable_task_tracker_.HasTrackedTasks());
  DCHECK_EQ(0UL, num_tasks_pending_);

  if (components_.empty()) {
    DCHECK(!done_callback_.is_null());
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(done_callback_));
    return;
  }

  // Allow blocking operations, such as file operations, in the component task.
  // This is allowed since there is no UI for it to block.
  auto task_runner =
      base::CreateTaskRunner({base::ThreadPool(), base::MayBlock()});
  for (auto& component : components_) {
    if (cancelable_task_tracker_.PostTaskAndReply(
            task_runner.get(), FROM_HERE,
            component_task.Run(component.get()),  // This returns a Closure.
            base::BindOnce(&ComponentManager::TaskCompleted,
                           base::Unretained(this))) ==
        base::CancelableTaskTracker::kBadTaskId) {
      PLOG(ERROR) << "Failed to run component's method: " << method_name;
    } else {
      // The reply task is never called synchronously, so it's OK to increment
      // the number of tasks pending after the task was posted.
      ++num_tasks_pending_;
    }
  }
}

void ComponentManager::TaskCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(num_tasks_pending_, 0UL);

  if (--num_tasks_pending_ == 0) {
    // The callback must be run asynchronously so that the task tracker is not
    // on the call stack anymore in cases where the callback ends up calling
    // CloseAllComponents.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(done_callback_));
  }
}

}  // namespace chrome_cleaner
