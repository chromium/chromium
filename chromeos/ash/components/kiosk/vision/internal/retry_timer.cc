// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/internal/retry_timer.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/syslog_logging.h"
#include "base/time/time.h"

namespace ash::kiosk_vision {

namespace {

// The delay to wait for before the very first retry.
constexpr auto kInitialDelay = base::Seconds(1);

// The maximum delay to wait for.
constexpr auto kMaxDelay = base::Minutes(5);

// The maximum value `run_count_` is allowed to increment to. Incrementing
// beyond this is not needed as `DelayFor(kMaxRunCount + 1)` crosses
// `kMaxDelay`.
constexpr int kMaxRunCount = std::bit_width((uint64_t)kMaxDelay.InSeconds());

base::TimeDelta DelayFor(int run_count) {
  return std::min(kMaxDelay, kInitialDelay * (1 << run_count));
}

}  // namespace

RetryTimer::RetryTimer() = default;

RetryTimer::~RetryTimer() = default;

void RetryTimer::Start(base::OnceClosure on_retry) {
  auto delay = DelayFor(run_count_);
  run_count_ = std::min(run_count_ + 1, kMaxRunCount);
  SYSLOG(INFO) << "Kiosk Vision will retry after a delay of " << delay;

  retry_timer_.Start(FROM_HERE, delay,
                     base::BindOnce(
                         [](base::OnceClosure on_retry) {
                           SYSLOG(INFO) << "Kiosk Vision retrying now...";
                           std::move(on_retry).Run();
                         },
                         std::move(on_retry)));
}

void RetryTimer::Stop() {
  retry_timer_.Stop();
}

}  // namespace ash::kiosk_vision
