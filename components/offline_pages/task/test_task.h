// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_TASK_TEST_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_TASK_TEST_TASK_H_

#include "base/memory/raw_ptr.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

// Sample resource consumed by the task during execution. In this set of tests
// used to provide the capability to continue the task.
class ConsumedResource {
 public:
  ConsumedResource();
  ~ConsumedResource();

  void Step(base::OnceClosure step_callback);
  void CompleteStep();
  bool HasNextStep() const { return !next_step_.is_null(); }

 private:
  base::OnceClosure next_step_;
};

// Sample test task. This should not be used as a example of task implementation
// with respect to callback safety. Otherwise it captures the idea of splitting
// the work into multiple steps separated by potentially asynchronous calls
// spanning multiple threads.
//
// For an implementation example of a task that covers problems better is
// |offline_pages::ChangeRequestsStateTask|.
class TestTask : public Task {
 public:
  enum class TaskState { NOT_STARTED, STEP_1, STEP_2, COMPLETED };

  explicit TestTask(ConsumedResource* resource);
  TestTask(ConsumedResource* resource, bool leave_early);
  ~TestTask() override;

  // Run is Step 1 in our case.
  void Run() override;

  void Step2();

  void LastStep();

  TaskState state() const { return state_; }

 private:
  raw_ptr<ConsumedResource> resource_;
  TaskState state_;
  bool leave_early_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_TASK_TEST_TASK_H_
