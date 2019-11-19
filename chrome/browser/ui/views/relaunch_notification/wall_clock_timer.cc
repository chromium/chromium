// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/wall_clock_timer.h"

#include <utility>

#include "base/power_monitor/power_monitor.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"

WallClockTimer::WallClockTimer() : WallClockTimer(nullptr, nullptr) {}

WallClockTimer::WallClockTimer(const base::Clock* clock,
                               const base::TickClock* tick_clock)
    : timer_(tick_clock),
      clock_(clock ? clock : base::DefaultClock::GetInstance()) {}

WallClockTimer::~WallClockTimer() {
  RemoveObserver();
}

void WallClockTimer::Start(const base::Location& posted_from,
                           base::Time desired_run_time,
                           base::OnceClosure user_task) {
  user_task_ = std::move(user_task);
  posted_from_ = posted_from;
  desired_run_time_ = desired_run_time;
  AddObserver();
  timer_.Start(posted_from_, desired_run_time_ - Now(), this,
               &WallClockTimer::RunUserTask);
}

base::Time WallClockTimer::Now() const {
  return clock_->Now();
}

void WallClockTimer::Stop() {
  timer_.Stop();
  user_task_.Reset();
  RemoveObserver();
}

bool WallClockTimer::IsRunning() {
  return timer_.IsRunning();
}

void WallClockTimer::RunUserTask() {
  DCHECK(user_task_);
  base::OnceClosure task = std::move(user_task_);
  std::move(task).Run();
  RemoveObserver();
}

void WallClockTimer::OnResume() {
  // This will actually restart timer with smaller delay
  timer_.Start(posted_from_, desired_run_time_ - Now(), this,
               &WallClockTimer::RunUserTask);
}

void WallClockTimer::AddObserver() {
  if (!observer_added_)
    observer_added_ = base::PowerMonitor::AddObserver(this);
}

void WallClockTimer::RemoveObserver() {
  if (observer_added_) {
    base::PowerMonitor::RemoveObserver(this);
    observer_added_ = false;
  }
}
