// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/timer_update_scheduler.h"

namespace component_updater {

TimerUpdateScheduler::TimerUpdateScheduler() = default;
TimerUpdateScheduler::~TimerUpdateScheduler() = default;

void TimerUpdateScheduler::Schedule(base::TimeDelta initial_delay,
                                    base::TimeDelta delay,
                                    const UserTask& user_task) {
  timer_.Start(initial_delay, delay, user_task);
}

void TimerUpdateScheduler::Stop() {
  timer_.Stop();
}

}  // namespace component_updater
