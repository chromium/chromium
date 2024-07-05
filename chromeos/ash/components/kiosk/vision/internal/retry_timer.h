// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_RETRY_TIMER_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_RETRY_TIMER_H_

#include "base/functional/callback_forward.h"
#include "base/timer/timer.h"

namespace ash::kiosk_vision {

// Implements an exponential backoff timer. Meant to be used to retry Kiosk
// Vision tasks after failure.
class RetryTimer {
 public:
  RetryTimer();
  RetryTimer(const RetryTimer&) = delete;
  RetryTimer& operator=(const RetryTimer&) = delete;
  ~RetryTimer();

  // Sets up `retry_timer_` to run `on_retry` after an exponential delay based
  // on `run_count_`.
  void Start(base::OnceClosure on_retry);

  // Stops `retry_timer_` if it is running. Does nothing otherwise.
  void Stop();

 private:
  int run_count_ = 0;
  base::OneShotTimer retry_timer_;
};

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_RETRY_TIMER_H_
