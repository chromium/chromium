// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_TEST_AUTOFILL_CLOCK_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_TEST_AUTOFILL_CLOCK_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace base {
class Clock;
class SimpleTestClock;
}  // namespace base

namespace autofill {

// Handles the customization of the time in tests. Replaces the clock in
// AutofillClock with a test version that can be manipulated from this class.
// Automatically resets a normal clock to AutofillClock when this gets
// destroyed.
//
// DEPRECATED: `AutofillClock` and `TestAutofillClockDeprecated` are being
// removed (crbug.com/40100455). Note that `TestAutofillClockDeprecated` only
// affects `AutofillClock::Now()`, not `base::Time::Now()`, which most Autofill
// code now uses.
//
// Instead:
// - In unit tests, configure `base::test::TaskEnvironment` with
//   `base::test::TaskEnvironment::TimeSource::MOCK_TIME` and advance time via
//   `FastForwardBy()` or `AdvanceClock()` (which mocks both `base::Time::Now()`
//   and `AutofillClock::Now()`).
// - In browser tests (where `MOCK_TIME` is unavailable), prefer relative
//   timestamps around `base::Time::Now()`, or use
//   `base::subtle::ScopedTimeClockOverrides` if overriding `base::Time::Now()`
//   is necessary.
//
// TODO(crbug.com/40100455): Remove this class.
class TestAutofillClockDeprecated {
 public:
  explicit TestAutofillClockDeprecated(base::Time now = {});
  explicit TestAutofillClockDeprecated(
      std::unique_ptr<base::SimpleTestClock> test_clock);

  TestAutofillClockDeprecated(const TestAutofillClockDeprecated&) = delete;
  TestAutofillClockDeprecated& operator=(const TestAutofillClockDeprecated&) =
      delete;

  ~TestAutofillClockDeprecated();

  // Set the time to be returned from AutofillClock::Now() calls.
  void SetNow(base::Time now);

  // Advances the clock by |delta|.
  void Advance(base::TimeDelta delta);

 private:
  std::unique_ptr<base::SimpleTestClock> test_clock_;
  raw_ptr<const base::Clock> previous_clock_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_TEST_AUTOFILL_CLOCK_H_
