// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_TEST_SCOPED_OFFLINE_CLOCK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_TEST_SCOPED_OFFLINE_CLOCK_H_

#include "base/test/simple_test_clock.h"

namespace offline_pages {

// Overrides |OfflineClock()| with |clock| upon construction. Returns
// |OfflineClock()| to its original state upon destruction.
class TestScopedOfflineClockOverride {
 public:
  explicit TestScopedOfflineClockOverride(const base::Clock* clock);

  TestScopedOfflineClockOverride(const TestScopedOfflineClockOverride&) =
      delete;
  TestScopedOfflineClockOverride& operator=(
      const TestScopedOfflineClockOverride&) = delete;

  ~TestScopedOfflineClockOverride();
};

// Overrides |OfflineClock()| with |this| upon construction. Returns
// |OfflineClock()| to its original state upon destruction.
class TestScopedOfflineClock : public base::SimpleTestClock {
 public:
  TestScopedOfflineClock();

  TestScopedOfflineClock(const TestScopedOfflineClock&) = delete;
  TestScopedOfflineClock& operator=(const TestScopedOfflineClock&) = delete;

  ~TestScopedOfflineClock() override;

 private:
  TestScopedOfflineClockOverride override_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_TEST_SCOPED_OFFLINE_CLOCK_H_
