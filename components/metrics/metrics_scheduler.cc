// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_scheduler.h"

#include "build/build_config.h"

namespace metrics {
namespace {

// The delay, in seconds, after startup before sending the first log message.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Sessions are more likely to be short on a mobile device, so handle the
// initial log quickly.
const int kInitialIntervalSeconds = 15;
#else
const int kInitialIntervalSeconds = 60;
#endif

}  // namespace

MetricsScheduler::MetricsScheduler(const base::RepeatingClosure& task_callback,
                                   bool fast_startup_for_testing)
    : task_callback_(task_callback),
      interval_(base::Seconds(
          fast_startup_for_testing ? 0 : kInitialIntervalSeconds)),
      running_(false),
      callback_pending_(false) {}

MetricsScheduler::~MetricsScheduler() = default;

void MetricsScheduler::Start() {
  running_ = true;
  ScheduleNextTask();
}

void MetricsScheduler::Stop() {
  running_ = false;
  if (timer_.IsRunning())
    timer_.Stop();
}

// static
int MetricsScheduler::GetInitialIntervalSeconds() {
  return kInitialIntervalSeconds;
}

void MetricsScheduler::TaskDone(base::TimeDelta next_interval) {
  DCHECK(callback_pending_);
  callback_pending_ = false;
  interval_ = next_interval;
  if (running_)
    ScheduleNextTask();
}

void MetricsScheduler::TriggerTask() {
  // This can happen in tests which set a very small timer interval.
  if (callback_pending_)
    return;

  callback_pending_ = true;
  task_callback_.Run();
}

void MetricsScheduler::ScheduleNextTask() {
  DCHECK(running_);
  if (timer_.IsRunning() || callback_pending_)
    return;

  timer_.Start(FROM_HERE, interval_, this, &MetricsScheduler::TriggerTask);
}

}  // namespace metrics
