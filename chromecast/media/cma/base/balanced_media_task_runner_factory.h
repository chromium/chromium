// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_BALANCED_MEDIA_TASK_RUNNER_FACTORY_H_
#define CHROMECAST_MEDIA_CMA_BASE_BALANCED_MEDIA_TASK_RUNNER_FACTORY_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromecast {
namespace media {
class BalancedMediaTaskRunner;
class MediaTaskRunner;

// BalancedMediaTaskRunnerFactory -
// Create media tasks runners that are loosely synchronized between each other.
// For two tasks T1 and T2 with timestamps ts1 and ts2, the scheduler ensures
// T2 is not scheduled before T1 if ts2 > ts1 + |max_delta|.
class BalancedMediaTaskRunnerFactory
    : public base::RefCountedThreadSafe<BalancedMediaTaskRunnerFactory> {
 public:
  explicit BalancedMediaTaskRunnerFactory(base::TimeDelta max_delta);

  BalancedMediaTaskRunnerFactory(const BalancedMediaTaskRunnerFactory&) =
      delete;
  BalancedMediaTaskRunnerFactory& operator=(
      const BalancedMediaTaskRunnerFactory&) = delete;

  // Creates a media task runner using |task_runner| as the underlying
  // regular task runner.
  // Restriction on the returned media task runner:
  // - can only schedule only one media task at a time.
  // - timestamps of tasks posted on that task runner must be increasing.
  scoped_refptr<MediaTaskRunner> CreateMediaTaskRunner(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

 private:
  typedef std::set<scoped_refptr<BalancedMediaTaskRunner> > MediaTaskRunnerSet;

  friend class base::RefCountedThreadSafe<BalancedMediaTaskRunnerFactory>;
  virtual ~BalancedMediaTaskRunnerFactory();

  // Invoked when one of the registered media task runners received a new media
  // task.
  void OnNewTask();

  // Unregister a media task runner.
  void UnregisterMediaTaskRunner(
      const scoped_refptr<BalancedMediaTaskRunner>& media_task_runner);

  // Maximum timestamp deviation between tasks from the registered task runners.
  const base::TimeDelta max_delta_;

  // Task runners created by the factory that have not been unregistered yet.
  base::Lock lock_;
  MediaTaskRunnerSet task_runners_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_BALANCED_MEDIA_TASK_RUNNER_FACTORY_H_
