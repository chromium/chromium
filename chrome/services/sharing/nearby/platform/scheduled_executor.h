// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_SCHEDULED_EXECUTOR_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_SCHEDULED_EXECUTOR_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/time/time.h"
#include "third_party/nearby/src/internal/platform/implementation/scheduled_executor.h"

namespace nearby::chrome {

// Concrete ScheduledExecutor implementation.
class ScheduledExecutor : public api::ScheduledExecutor {
 public:
  explicit ScheduledExecutor(
      scoped_refptr<base::SequencedTaskRunner> timer_task_runner);
  ~ScheduledExecutor() override;

  ScheduledExecutor(const ScheduledExecutor&) = delete;
  ScheduledExecutor& operator=(const ScheduledExecutor&) = delete;

  // api::ScheduledExecutor:
  std::shared_ptr<api::Cancelable> Schedule(Runnable&& runnable,
                                            absl::Duration duration) override;
  void Execute(Runnable&& runnable) override;
  void Shutdown() override;

 private:
  struct PendingTaskWithTimer {
    explicit PendingTaskWithTimer(Runnable&& runnable);
    ~PendingTaskWithTimer();

    Runnable runnable;
    base::OneShotTimer timer;
  };

  // Static wrapper that simply runs OnTaskCancelled() if the exector has not
  // already been destroyed. This is necessary because
  // base::WeakPtr<ScheduledExecutor> is not allowed to bind to
  // OnTaskCancelled(), which is non-static and non-void.
  static bool TryCancelTask(base::WeakPtr<ScheduledExecutor> executor,
                            const base::UnguessableToken& id);

  // To ensure thread-safety, these methods are only to be posted as tasks on
  // |timer_api_task_runner_| so that they execute in the same sequence.
  void StartTimerWithId(const base::UnguessableToken& id,
                        base::TimeDelta delay);
  void StopTimerWithIdAndDeleteTaskEntry(const base::UnguessableToken& id);

  void RunTaskWithId(const base::UnguessableToken& id);
  void RemoveTaskEntryWithId(const base::UnguessableToken& id);
  bool OnTaskCancelled(const base::UnguessableToken& id);

  // SequencedTaskRunner that all base::OneShotTimer method calls (e.g. Start()
  // and Stop()) need to be run on, to ensure thread-safety. This is also where
  // tasks posted to base::OneShotTimer will run.
  scoped_refptr<base::SequencedTaskRunner> timer_task_runner_;

  base::Lock lock_;
  // Tracks if the executor has been shutdown. Accessed from different threads
  // through public APIs and task_runner_.
  bool is_shut_down_ GUARDED_BY(lock_) = false;
  // Tracks all pending tasks. Accessed from different threads through public
  // APIs and task_runner_.
  std::map<base::UnguessableToken, std::unique_ptr<PendingTaskWithTimer>>
      id_to_task_map_ GUARDED_BY(lock_);
  SEQUENCE_CHECKER(timer_sequence_checker_);
  // WeakPtrFactory bound to |timer_task_runer_| to prevent use-after-free.
  base::WeakPtrFactory<ScheduledExecutor> timer_task_runner_weak_factory_{this};
  // WeakPtrFactory bound to Cancelable task
  base::WeakPtrFactory<ScheduledExecutor> cancelable_task_weak_factory_{this};
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_SCHEDULED_EXECUTOR_H_
