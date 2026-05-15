// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/inactivity_timer.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/time.h"

namespace storage {

namespace {
// Number of strikes to divide the total delay into.
constexpr int kMaxStrikes = 3;
}  // namespace

InactivityTimer::InactivityTimer() = default;

InactivityTimer::InactivityTimer(const base::Location& posted_from,
                                 base::TimeDelta delay,
                                 base::RepeatingClosure action)
    : action_(std::move(action)),
      timer_(posted_from,
             delay / kMaxStrikes,
             base::BindRepeating(&InactivityTimer::OnTimerFired,
                                 base::Unretained(this))) {
  CHECK(action_);
}

InactivityTimer::~InactivityTimer() = default;

void InactivityTimer::Start(const base::Location& posted_from,
                            base::TimeDelta delay,
                            base::RepeatingClosure action) {
  CHECK(action);
  action_ = std::move(action);
  strikes_ = 0;
  timer_.Start(posted_from, delay / kMaxStrikes,
               base::BindRepeating(&InactivityTimer::OnTimerFired,
                                   base::Unretained(this)));
}

void InactivityTimer::Stop() {
  strikes_ = 0;
  timer_.Stop();
}

void InactivityTimer::Reset() {
  CHECK(action_);
  strikes_ = 0;
  timer_.Reset();
}

bool InactivityTimer::IsRunning() const {
  return timer_.IsRunning();
}

base::TimeTicks InactivityTimer::ExpectedFiringTimeForTesting() const {
  CHECK(IsRunning());
  return timer_.desired_run_time() +
         timer_.GetCurrentDelay() * (kMaxStrikes - strikes_ - 1);
}

void InactivityTimer::OnTimerFired() {
  if (++strikes_ == kMaxStrikes) {
    strikes_ = 0;
    timer_.Stop();
    action_.Run();
  }
}

}  // namespace storage
