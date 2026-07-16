// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/test_support/wait_for.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/pending_task.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/task_observer.h"

namespace browser_actuator {

namespace {

class ConditionWaiter : public base::TaskObserver {
 public:
  ConditionWaiter(base::FunctionRef<bool()> condition, base::OnceClosure quit)
      : condition_(condition), quit_(std::move(quit)) {
    base::CurrentThread::Get()->AddTaskObserver(this);
  }
  ~ConditionWaiter() override {
    base::CurrentThread::Get()->RemoveTaskObserver(this);
  }

  bool satisfied() const { return satisfied_; }

  // base::TaskObserver:
  void WillProcessTask(const base::PendingTask&, bool) override {}
  void DidProcessTask(const base::PendingTask&) override {
    if (satisfied_ || !quit_) {
      return;
    }
    if (condition_()) {
      satisfied_ = true;
      std::move(quit_).Run();
    } else if (--task_budget_ == 0) {
      // A mock clock can feed timer cycles forever; fail rather than hang.
      std::move(quit_).Run();
    }
  }

 private:
  base::FunctionRef<bool()> condition_;
  base::OnceClosure quit_;
  bool satisfied_ = false;
  int task_budget_ = 1000000;
};

}  // namespace

bool WaitFor(base::FunctionRef<bool()> condition) {
  if (condition()) {
    return true;
  }
  base::RunLoop run_loop;
  ConditionWaiter waiter(condition, run_loop.QuitClosure());
  run_loop.Run();
  return waiter.satisfied();
}

}  // namespace browser_actuator
