// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_request_scheduler_mobile.h"

#include "base/command_line.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_switches.h"

namespace variations {

namespace {

// Time before attempting a seed fetch after a ScheduleFetch(), in seconds.
const int kScheduleFetchDelaySeconds = 5;

}  // namespace

VariationsRequestSchedulerMobile::VariationsRequestSchedulerMobile(
    const base::RepeatingClosure& task,
    PrefService* local_state)
    : VariationsRequestScheduler(task), local_state_(local_state) {}

VariationsRequestSchedulerMobile::~VariationsRequestSchedulerMobile() = default;

void VariationsRequestSchedulerMobile::Start() {
  // Check the time of the last request. If it has been longer than the fetch
  // period, run the task. Otherwise, do nothing. Note that no future requests
  // are scheduled since it is unlikely that the mobile process would live long
  // enough for the timer to fire.
  const base::Time last_fetch_time =
      local_state_->GetTime(prefs::kVariationsLastFetchTime);
  if (ShouldFetchSeed(last_fetch_time)) {
    last_request_time_ = base::Time::Now();
    task().Run();
  }
}

void VariationsRequestSchedulerMobile::Reset() {
}

void VariationsRequestSchedulerMobile::OnAppEnterForeground() {
  // Verify we haven't just attempted a fetch (which has not completed). This
  // is mainly used to verify we don't trigger a second fetch for the
  // OnAppEnterForeground right after startup.
  if (!ShouldFetchSeed(last_request_time_))
    return;

  // Since Start() launches a one-off execution, we can reuse it here. Also
  // note that since Start() verifies that the seed needs to be refreshed, we
  // do not verify here.
  schedule_fetch_timer_.Start(FROM_HERE,
                              base::Seconds(kScheduleFetchDelaySeconds), this,
                              &VariationsRequestSchedulerMobile::Start);
}

bool VariationsRequestSchedulerMobile::ShouldFetchSeed(
    base::Time last_fetch_time) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableVariationsSeedFetchThrottling)) {
    return true;
  }
  return base::Time::Now() > (last_fetch_time + GetFetchPeriod());
}

// static
VariationsRequestScheduler* VariationsRequestScheduler::Create(
    const base::RepeatingClosure& task,
    PrefService* local_state) {
  return new VariationsRequestSchedulerMobile(task, local_state);
}

}  // namespace variations
