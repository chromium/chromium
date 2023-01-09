// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_delay_based_time_source.h"

#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"

namespace viz {

void FakeDelayBasedTimeSourceClient::OnTimerTick() {
  tick_called_ = true;
}

FakeDelayBasedTimeSource::FakeDelayBasedTimeSource(
    const base::TickClock* now_src,
    base::SingleThreadTaskRunner* task_runner)
    : DelayBasedTimeSource(task_runner), now_src_(now_src) {}

base::TimeTicks FakeDelayBasedTimeSource::Now() const {
  return now_src_->NowTicks();
}

std::string FakeDelayBasedTimeSource::TypeString() const {
  return "FakeDelayBasedTimeSource";
}

FakeDelayBasedTimeSource::~FakeDelayBasedTimeSource() = default;

}  // namespace viz
