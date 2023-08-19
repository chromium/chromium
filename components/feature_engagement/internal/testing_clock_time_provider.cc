// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/testing_clock_time_provider.h"

#include "base/check_is_test.h"
#include "base/numerics/clamped_math.h"
#include "base/time/clock.h"
#include "base/time/time.h"

namespace feature_engagement {

TestingClockTimeProvider::TestingClockTimeProvider(const base::Clock& clock,
                                                   base::Time initial_now)
    : clock_(clock), initial_now_(initial_now) {
  CHECK_IS_TEST();
}

TestingClockTimeProvider::~TestingClockTimeProvider() = default;

uint32_t TestingClockTimeProvider::GetCurrentDay() const {
  return base::saturated_cast<uint32_t>((Now() - initial_now_).InDays());
}

base::Time TestingClockTimeProvider::Now() const {
  return clock_.Now();
}

}  // namespace feature_engagement
