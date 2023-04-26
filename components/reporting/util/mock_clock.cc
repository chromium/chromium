// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/mock_clock.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/time/time_override.h"

namespace reporting::test {

// static
MockClock& MockClock::Get() {
  static base::NoDestructor<MockClock> mock_clock;
  return *mock_clock;
}

void MockClock::Advance(base::TimeDelta duration) {
  base::AutoLock lock(lock_);
  offset_ += duration;
}

// static
base::Time MockClock::MockedNow() {
  return base::subtle::TimeNowIgnoringOverride() + Get().Offset();
}

// static
base::TimeTicks MockClock::MockedTicksNow() {
  return base::subtle::TimeTicksNowIgnoringOverride() + Get().Offset();
}

MockClock::MockClock()
    : time_override_(std::make_unique<base::subtle::ScopedTimeClockOverrides>(
          &MockClock::MockedNow,
          &MockClock::MockedTicksNow,
          nullptr)) {}

MockClock::~MockClock() = default;

base::TimeDelta MockClock::Offset() {
  base::AutoLock lock(lock_);
  return offset_;
}

}  // namespace reporting::test
