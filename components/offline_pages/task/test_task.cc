// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/test_task.h"

#include "base/functional/bind.h"

namespace offline_pages {

ConsumedResource::ConsumedResource() = default;

ConsumedResource::~ConsumedResource() = default;

void ConsumedResource::Step(base::OnceClosure step_callback) {
  next_step_ = std::move(step_callback);
}

void ConsumedResource::CompleteStep() {
  std::move(next_step_).Run();
}

TestTask::TestTask(ConsumedResource* resource)
    : resource_(resource),
      state_(TaskState::NOT_STARTED),
      leave_early_(false) {}

TestTask::TestTask(ConsumedResource* resource, bool leave_early)
    : resource_(resource),
      state_(TaskState::NOT_STARTED),
      leave_early_(leave_early) {}

TestTask::~TestTask() = default;

// Run is Step 1 in our case.
void TestTask::Run() {
  state_ = TaskState::STEP_1;
  resource_->Step(base::BindOnce(&TestTask::Step2, base::Unretained(this)));
}

void TestTask::Step2() {
  if (leave_early_) {
    LastStep();
    return;
  }
  state_ = TaskState::STEP_2;
  resource_->Step(base::BindOnce(&TestTask::LastStep, base::Unretained(this)));
}

// This is step 3, but we conclude here.
void TestTask::LastStep() {
  state_ = TaskState::COMPLETED;
  TaskComplete();
}

}  // namespace offline_pages
