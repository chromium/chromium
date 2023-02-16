// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_queue.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

RemoteCommandsQueue::RemoteCommandsQueue()
    : clock_(base::DefaultClock::GetInstance()),
      tick_clock_(base::DefaultTickClock::GetInstance()) {}

RemoteCommandsQueue::~RemoteCommandsQueue() {
  while (!incoming_commands_.empty()) {
    incoming_commands_.pop();
  }
  if (running_command_) {
    running_command_->Terminate();
  }
}

void RemoteCommandsQueue::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void RemoteCommandsQueue::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void RemoteCommandsQueue::AddJob(std::unique_ptr<RemoteCommandJob> job) {
  incoming_commands_.emplace(std::move(job));

  if (!running_command_) {
    ScheduleNextJob();
  }
}

void RemoteCommandsQueue::SetClocksForTesting(
    const base::Clock* clock,
    const base::TickClock* tick_clock) {
  clock_ = clock;
  tick_clock_ = tick_clock;
}

base::TimeTicks RemoteCommandsQueue::GetNowTicks() {
  return tick_clock_->NowTicks();
}

void RemoteCommandsQueue::OnCommandTimeout() {
  DCHECK(running_command_);

  // Calling Terminate() will also trigger CurrentJobFinished() below.
  running_command_->Terminate();
}

void RemoteCommandsQueue::CurrentJobFinished() {
  DCHECK(running_command_);

  execution_timeout_timer_.Stop();

  for (auto& observer : observer_list_) {
    observer.OnJobFinished(running_command_.get());
  }
  running_command_.reset();

  ScheduleNextJob();
}

void RemoteCommandsQueue::ScheduleNextJob() {
  DCHECK(!running_command_);
  if (incoming_commands_.empty()) {
    return;
  }
  DCHECK(!execution_timeout_timer_.IsRunning());

  running_command_ = std::move(incoming_commands_.front());
  incoming_commands_.pop();

  execution_timeout_timer_.Start(FROM_HERE,
                                 running_command_->GetCommandTimeout(), this,
                                 &RemoteCommandsQueue::OnCommandTimeout);

  if (running_command_->Run(
          clock_->Now(), tick_clock_->NowTicks(),
          base::BindOnce(&RemoteCommandsQueue::CurrentJobFinished,
                         base::Unretained(this)))) {
    for (auto& observer : observer_list_) {
      observer.OnJobStarted(running_command_.get());
    }
  } else {
    CurrentJobFinished();
  }
}

}  // namespace policy
