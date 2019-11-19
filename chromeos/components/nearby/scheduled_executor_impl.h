// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_SCHEDULED_EXECUTOR_IMPL_H_
#define CHROMEOS_COMPONENTS_NEARBY_SCHEDULED_EXECUTOR_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/components/nearby/library/atomic_boolean.h"
#include "chromeos/components/nearby/library/cancelable.h"
#include "chromeos/components/nearby/library/runnable.h"
#include "chromeos/components/nearby/library/scheduled_executor.h"

namespace chromeos {

namespace nearby {

// TODO(kyleqian): Use Ptr once the Nearby library is merged in.
// Concrete location::nearby::ScheduledExecutor implementation.
class ScheduledExecutorImpl : public location::nearby::ScheduledExecutor {
 public:
  ScheduledExecutorImpl(
      scoped_refptr<base::SequencedTaskRunner> timer_task_runner =
          base::CreateSequencedTaskRunner({base::ThreadPool(),
                                           base::MayBlock()}));
  ~ScheduledExecutorImpl() override;

 private:
  struct PendingTaskWithTimer {
    PendingTaskWithTimer(std::shared_ptr<location::nearby::Runnable> runnable,
                         std::unique_ptr<base::OneShotTimer> timer);
    ~PendingTaskWithTimer();
    std::shared_ptr<location::nearby::Runnable> runnable;
    std::unique_ptr<base::OneShotTimer> timer;
  };

  // Static wrapper that simply runs OnTaskCancelled() if the exector has not
  // already been destroyed. This is necessary because
  // base::WeakPtr<ScheduledExecutorImpl> is not allowed to bind to
  // OnTaskCancelled(), which is non-static and non-void.
  static bool TryCancelTask(base::WeakPtr<ScheduledExecutorImpl> executor,
                            const base::UnguessableToken& id);

  // location::nearby::Executor:
  void shutdown() override;

  // location::nearby::ScheduledExecutor:
  std::shared_ptr<location::nearby::Cancelable> schedule(
      std::shared_ptr<location::nearby::Runnable> runnable,
      int64_t delay_millis) override;

  // To ensure thread-safety, these methods are only to be posted as tasks on
  // |timer_api_task_runner_| so that they execute in the same sequence.
  void StartTimerWithId(const base::UnguessableToken& id,
                        const base::TimeDelta& delay);
  void StopTimerWithIdAndDeleteTaskEntry(const base::UnguessableToken& id);

  void RunTaskWithId(const base::UnguessableToken& id);
  void RemoveTaskEntryWithId(const base::UnguessableToken& id);
  bool OnTaskCancelled(const base::UnguessableToken& id);

  // SequencedTaskRunner that all base::OneShotTimer method calls (e.g. Start()
  // and Stop()) need to be run on, to ensure thread-safety. This is also where
  // tasks posted to base::OneShotTimer will run.
  scoped_refptr<base::SequencedTaskRunner> timer_task_runner_;

  std::unique_ptr<location::nearby::AtomicBoolean> is_shut_down_;
  base::Lock map_lock_;
  base::flat_map<base::UnguessableToken, std::unique_ptr<PendingTaskWithTimer>>
      id_to_task_map_;
  SEQUENCE_CHECKER(timer_sequence_checker_);
  base::WeakPtrFactory<ScheduledExecutorImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ScheduledExecutorImpl);
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_SCHEDULED_EXECUTOR_IMPL_H_
