// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/balanced_media_task_runner_factory.h"

#include <map>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/media/cma/base/media_task_runner.h"
#include "media/base/timestamp_constants.h"

namespace chromecast {
namespace media {

// MediaTaskRunnerWithNotification -
// Media task runner which also behaves as a media task runner observer.
class MediaTaskRunnerWithNotification : public MediaTaskRunner {
 public:
  // Wraps a MediaTaskRunner so that a third party can:
  // - be notified when a PostMediaTask is performed on this media task runner.
  //   |new_task_cb| is invoked in that case.
  // - monitor the lifetime of the media task runner, i.e. check when the media
  //   task runner is not needed anymore.
  //   |shutdown_cb| is invoked in that case.
  MediaTaskRunnerWithNotification(
      const scoped_refptr<MediaTaskRunner>& media_task_runner,
      base::RepeatingClosure new_task_cb,
      base::OnceClosure shutdown_cb);

  MediaTaskRunnerWithNotification(const MediaTaskRunnerWithNotification&) =
      delete;
  MediaTaskRunnerWithNotification& operator=(
      const MediaTaskRunnerWithNotification&) = delete;

  // MediaTaskRunner implementation.
  bool PostMediaTask(const base::Location& from_here,
                     base::OnceClosure task,
                     base::TimeDelta timestamp) override;

 private:
  ~MediaTaskRunnerWithNotification() override;

  scoped_refptr<MediaTaskRunner> const media_task_runner_;

  const base::RepeatingClosure new_task_cb_;
  base::OnceClosure shutdown_cb_;
};

MediaTaskRunnerWithNotification::MediaTaskRunnerWithNotification(
    const scoped_refptr<MediaTaskRunner>& media_task_runner,
    base::RepeatingClosure new_task_cb,
    base::OnceClosure shutdown_cb)
    : media_task_runner_(media_task_runner),
      new_task_cb_(std::move(new_task_cb)),
      shutdown_cb_(std::move(shutdown_cb)) {}

MediaTaskRunnerWithNotification::~MediaTaskRunnerWithNotification() {
  std::move(shutdown_cb_).Run();
}

bool MediaTaskRunnerWithNotification::PostMediaTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta timestamp) {
  bool may_run_in_future =
      media_task_runner_->PostMediaTask(from_here, std::move(task), timestamp);
  if (may_run_in_future)
    new_task_cb_.Run();
  return may_run_in_future;
}

// BalancedMediaTaskRunner -
// Run media tasks whose timestamp is less or equal to a max timestamp.
//
// Restrictions of BalancedMediaTaskRunner:
// - Can have at most one task in the queue.
// - Tasks should be given by increasing timestamps.
class BalancedMediaTaskRunner
    : public MediaTaskRunner {
 public:
  explicit BalancedMediaTaskRunner(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  BalancedMediaTaskRunner(const BalancedMediaTaskRunner&) = delete;
  BalancedMediaTaskRunner& operator=(const BalancedMediaTaskRunner&) = delete;

  // Schedule tasks whose timestamp is less than or equal to |max_timestamp|.
  void ScheduleWork(base::TimeDelta max_timestamp);

  // Return the timestamp of the last media task.
  // Return ::media::kNoTimestamp if no media task has been posted.
  base::TimeDelta GetMediaTimestamp() const;

  // MediaTaskRunner implementation.
  bool PostMediaTask(const base::Location& from_here,
                     base::OnceClosure task,
                     base::TimeDelta timestamp) override;

 private:
  ~BalancedMediaTaskRunner() override;

  scoped_refptr<base::SingleThreadTaskRunner> const task_runner_;

  // Protects the following variables.
  mutable base::Lock lock_;

  // Possible pending media task.
  base::Location from_here_;
  base::OnceClosure pending_task_;

  // Timestamp of the last posted task.
  // Is initialized to ::media::kNoTimestamp.
  base::TimeDelta last_timestamp_;
};

BalancedMediaTaskRunner::BalancedMediaTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner), last_timestamp_(::media::kNoTimestamp) {}

BalancedMediaTaskRunner::~BalancedMediaTaskRunner() {
}

void BalancedMediaTaskRunner::ScheduleWork(base::TimeDelta max_media_time) {
  base::OnceClosure task;
  {
    base::AutoLock auto_lock(lock_);
    if (pending_task_.is_null())
      return;

    if (last_timestamp_ != ::media::kNoTimestamp &&
        last_timestamp_ >= max_media_time) {
      return;
    }

    task = std::move(pending_task_);
  }
  task_runner_->PostTask(from_here_, std::move(task));
}

base::TimeDelta BalancedMediaTaskRunner::GetMediaTimestamp() const {
  base::AutoLock auto_lock(lock_);
  return last_timestamp_;
}

bool BalancedMediaTaskRunner::PostMediaTask(const base::Location& from_here,
                                            base::OnceClosure task,
                                            base::TimeDelta timestamp) {
  DCHECK(!task.is_null());

  // Pass through for a task with no timestamp.
  if (timestamp == ::media::kNoTimestamp) {
    return task_runner_->PostTask(from_here, std::move(task));
  }

  base::AutoLock auto_lock(lock_);

  // Timestamps must be in order.
  // Any task that does not meet that condition is simply discarded.
  if (last_timestamp_ != ::media::kNoTimestamp && timestamp < last_timestamp_) {
    return false;
  }

  // Only support one pending task at a time.
  DCHECK(pending_task_.is_null());
  from_here_ = from_here;
  pending_task_ = std::move(task);
  last_timestamp_ = timestamp;

  return true;
}

BalancedMediaTaskRunnerFactory::BalancedMediaTaskRunnerFactory(
    base::TimeDelta max_delta)
  : max_delta_(max_delta) {
}

BalancedMediaTaskRunnerFactory::~BalancedMediaTaskRunnerFactory() {
}

scoped_refptr<MediaTaskRunner>
BalancedMediaTaskRunnerFactory::CreateMediaTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  scoped_refptr<BalancedMediaTaskRunner> media_task_runner(
      new BalancedMediaTaskRunner(task_runner));
  scoped_refptr<MediaTaskRunnerWithNotification> media_task_runner_wrapper(
      new MediaTaskRunnerWithNotification(
          media_task_runner,
          base::BindRepeating(&BalancedMediaTaskRunnerFactory::OnNewTask, this),
          base::BindOnce(
              &BalancedMediaTaskRunnerFactory::UnregisterMediaTaskRunner, this,
              media_task_runner)));
  base::AutoLock auto_lock(lock_);
  // Note that |media_task_runner| is inserted here and
  // not |media_task_runner_wrapper|. Otherwise, we would always have one
  // ref on |media_task_runner_wrapper| and would never get the release
  // notification.
  // When |media_task_runner_wrapper| is going away,
  // BalancedMediaTaskRunnerFactory will receive a notification and will in
  // turn remove |media_task_runner|.
  task_runners_.insert(media_task_runner);
  return media_task_runner_wrapper;
}

void BalancedMediaTaskRunnerFactory::OnNewTask() {
  typedef
      std::multimap<base::TimeDelta, scoped_refptr<BalancedMediaTaskRunner> >
      TaskRunnerMap;
  TaskRunnerMap runnable_task_runner;

  base::AutoLock auto_lock(lock_);

  // Get the minimum timestamp among all streams.
  for (MediaTaskRunnerSet::const_iterator it = task_runners_.begin();
       it != task_runners_.end(); ++it) {
    base::TimeDelta timestamp((*it)->GetMediaTimestamp());
    if (timestamp == ::media::kNoTimestamp)
      continue;
    runnable_task_runner.insert(
        std::pair<base::TimeDelta, scoped_refptr<BalancedMediaTaskRunner> >(
            timestamp, *it));
  }

  // If there is no media task, just returns.
  if (runnable_task_runner.empty())
    return;

  // Run tasks which meet the balancing criteria.
  base::TimeDelta min_timestamp(runnable_task_runner.begin()->first);
  base::TimeDelta max_timestamp = min_timestamp + max_delta_;
  for (TaskRunnerMap::iterator it = runnable_task_runner.begin();
       it != runnable_task_runner.end(); ++it) {
    (*it).second->ScheduleWork(max_timestamp);
  }
}

void BalancedMediaTaskRunnerFactory::UnregisterMediaTaskRunner(
      const scoped_refptr<BalancedMediaTaskRunner>& media_task_runner) {
  {
    base::AutoLock auto_lock(lock_);
    task_runners_.erase(media_task_runner);
  }
  // After removing one of the task runners some of the other task runners might
  // need to be waken up, if they are no longer blocked by the balancing
  // restrictions with the old stream.
  OnNewTask();
}

}  // namespace media
}  // namespace chromecast
