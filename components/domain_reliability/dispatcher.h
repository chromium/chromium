// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_DISPATCHER_H_
#define COMPONENTS_DOMAIN_RELIABILITY_DISPATCHER_H_

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/domain_reliability/domain_reliability_export.h"

namespace domain_reliability {

class MockableTime;

// Runs tasks during a specified interval. Calling |RunEligibleTasks| gives any
// task a chance to run early (if the minimum delay has already passed); tasks
// that aren't run early will be run once their maximum delay has passed.
//
// (See scheduler.h for an explanation of how the intervals are chosen.)
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityDispatcher {
 public:
  explicit DomainReliabilityDispatcher(MockableTime* time);
  ~DomainReliabilityDispatcher();

  // Schedules |task| to be executed between |min_delay| and |max_delay| from
  // now. The task will be run at most |max_delay| from now; once |min_delay|
  // has passed, any call to |RunEligibleTasks| will run the task earlier than
  // that.
  void ScheduleTask(const base::Closure& task,
                    base::TimeDelta min_delay,
                    base::TimeDelta max_delay);

  // Runs all existing tasks whose minimum delay has already passed. Does not
  // run tasks added by those existing tasks, even if their minimum delay has
  // already passed.
  void RunEligibleTasks();

  // Runs all waiting or eligible tasks, regardless of whether their minimum
  // delay has passed.
  void RunAllTasksForTesting();

 private:
  struct Task;

  // Adds |task| to the set of all tasks, but not the set of eligible tasks.
  void MakeTaskWaiting(Task* task);

  // Adds |task| to the set of eligible tasks, and also the set of all tasks
  // if not already there.
  void MakeTaskEligible(Task* task);

  // Runs |task|'s callback, removes it from both sets, and deletes it.
  void RunAndDeleteTask(Task* task);

  MockableTime* time_;
  std::set<std::unique_ptr<Task>, base::UniquePtrComparator> tasks_;
  std::set<Task*> eligible_tasks_;

  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityDispatcher);
};

}  // namespace domain_reliability

#endif
