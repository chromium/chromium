// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/alarm_manager.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"

#define MAKE_SURE_OWN_THREAD(callback, ...)                                    \
  if (!task_runner_->BelongsToCurrentThread()) {                               \
    task_runner_->PostTask(                                                    \
        FROM_HERE, base::BindOnce(&AlarmManager::callback,                     \
                                  weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                    \
  }

namespace chromecast {

namespace {
int kClockPollInterval = 5;

void VerifyHandleCallback(base::OnceClosure task,
                          base::WeakPtr<AlarmHandle> handle) {
  if (!handle.get()) {
    return;
  }
  std::move(task).Run();
}
}  // namespace

AlarmHandle::AlarmHandle() = default;
AlarmHandle::~AlarmHandle() = default;

AlarmManager::AlarmInfo::AlarmInfo(
    base::OnceClosure task,
    base::Time time,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_(std::move(task)),
      time_(time),
      task_runner_(std::move(task_runner)) {}

AlarmManager::AlarmInfo::~AlarmInfo() {}

void AlarmManager::AlarmInfo::PostTask() {
  task_runner_->PostTask(FROM_HERE, std::move(task_));
}

AlarmManager::AlarmManager(
    const base::Clock* clock,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : clock_(clock),
      task_runner_(std::move(task_runner)),
      weak_factory_(this) {
  DCHECK(clock_);
  DCHECK(task_runner_);
  clock_tick_timer_.SetTaskRunner(task_runner_);
  base::TimeDelta polling_frequency = base::Seconds(kClockPollInterval);
  clock_tick_timer_.Start(FROM_HERE, polling_frequency,
                          base::BindRepeating(&AlarmManager::CheckAlarm,
                                              weak_factory_.GetWeakPtr()));
}

AlarmManager::AlarmManager()
    : AlarmManager(base::DefaultClock::GetInstance(),
                   base::SingleThreadTaskRunner::GetCurrentDefault()) {}

AlarmManager::~AlarmManager() {}

std::unique_ptr<AlarmHandle> AlarmManager::PostAlarmTask(base::OnceClosure task,
                                                         base::Time time) {
  DCHECK(task);
  std::unique_ptr<AlarmHandle> handle = std::make_unique<AlarmHandle>();
  AddAlarm(base::BindOnce(&VerifyHandleCallback, std::move(task),
                          handle->AsWeakPtr()),
           time, base::SingleThreadTaskRunner::GetCurrentDefault());
  return handle;
}

void AlarmManager::AddAlarm(
    base::OnceClosure task,
    base::Time time,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  MAKE_SURE_OWN_THREAD(AddAlarm, std::move(task), time, std::move(task_runner));
  next_alarm_.push(std::make_unique<AlarmInfo>(std::move(task), time,
                                               std::move(task_runner)));
}

void AlarmManager::CheckAlarm() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::Time now = clock_->Now();
  // Fire appropriate alarms.
  while (!next_alarm_.empty() && now >= next_alarm_.top()->time()) {
    next_alarm_.top()->PostTask();
    next_alarm_.pop();
  }
}

}  // namespace chromecast
