// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_MOCK_CLOCK_H_
#define COMPONENTS_REPORTING_UTIL_MOCK_CLOCK_H_

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/time/time_override.h"

namespace reporting::test {

// Mock clock that supplies a mock time and can be advanced for testing
// purposes. Only applicable for browser tests where a `TaskEnvironment` is
// unavailable.
class MockClock {
 public:
  // Returns a MockClock which will not be deleted. Should be called once while
  // single-threaded to initialize ScopedTimeClockOverrides to avoid threading
  // issues.
  static MockClock& Get();

  MockClock(const MockClock&) = delete;
  MockClock& operator=(const MockClock&) = delete;
  ~MockClock();

  // Advances clock by the specified duration.
  void Advance(base::TimeDelta duration);

 private:
  friend base::NoDestructor<MockClock>;

  // Static helpers that return the current time based on the offset duration.
  // This is used to set up the time override.
  static base::Time MockedNow();
  static base::TimeTicks MockedTicksNow();

  MockClock();

  // Returns the offset duration.
  base::TimeDelta Offset();
  const std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_override_;

  // A lock is necessary because `MockedNow` and `MockedTicksNow` can be
  // accessed by components from different threads when they retrieve the
  // current time.
  base::Lock lock_;
  base::TimeDelta offset_ GUARDED_BY(lock_);
};

}  // namespace reporting::test

#endif  // COMPONENTS_REPORTING_UTIL_MOCK_CLOCK_H_
