// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/persistent_repeating_timer.h"

#include "base/callback.h"
#include "components/prefs/pref_service.h"

namespace signin {

PersistentRepeatingTimer::PersistentRepeatingTimer(
    PrefService* prefs,
    const char* timer_last_update_pref_name,
    base::TimeDelta delay,
    base::RepeatingClosure task)
    : prefs_(prefs),
      last_fired_pref_name_(timer_last_update_pref_name),
      delay_(delay),
      user_task_(task) {}

PersistentRepeatingTimer::~PersistentRepeatingTimer() = default;

void PersistentRepeatingTimer::Start() {
  if (timer_.IsRunning())
    return;  // Already started.

  const base::TimeDelta time_since_update = base::Time::Now() - GetLastFired();
  if (time_since_update >= delay_) {
    OnTimerFired();
  } else {
    timer_.Start(FROM_HERE, delay_ - time_since_update,
                 base::BindRepeating(&PersistentRepeatingTimer::OnTimerFired,
                                     base::Unretained(this)));
  }
  DCHECK(timer_.IsRunning());
}

base::Time PersistentRepeatingTimer::GetLastFired() {
  return prefs_->GetTime(last_fired_pref_name_);
}

void PersistentRepeatingTimer::SetLastFiredNow() {
  prefs_->SetTime(last_fired_pref_name_, base::Time::Now());
}

void PersistentRepeatingTimer::OnTimerFired() {
  DCHECK(!timer_.IsRunning());
  SetLastFiredNow();
  user_task_.Run();
  Start();
}

}  // namespace signin
