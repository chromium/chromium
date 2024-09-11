// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/idle_detector/idle_detector.h"

#include "base/location.h"
#include "base/time/default_tick_clock.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ash {

IdleDetector::IdleDetector(const base::RepeatingClosure& on_idle_callback,
                           const base::TickClock* tick_clock)
    : timer_(tick_clock), idle_callback_(on_idle_callback) {}

IdleDetector::~IdleDetector() {
  ui::UserActivityDetector* user_activity_detector =
      ui::UserActivityDetector::Get();
  if (user_activity_detector->HasObserver(this)) {
    user_activity_detector->RemoveObserver(this);
  }
}

void IdleDetector::OnUserActivity(const ui::Event* event) {
  ResetTimer();
}

void IdleDetector::Start(const base::TimeDelta& timeout) {
  timeout_ = timeout;
  ui::UserActivityDetector* user_activity_detector =
      ui::UserActivityDetector::Get();
  if (!user_activity_detector->HasObserver(this)) {
    user_activity_detector->AddObserver(this);
  }
  ResetTimer();
}

void IdleDetector::ResetTimer() {
  if (timer_.IsRunning()) {
    timer_.Reset();
  } else {
    timer_.Start(FROM_HERE, timeout_, idle_callback_);
  }
}

}  // namespace ash
