// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLOCK_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLOCK_H_

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
class TestAutofillClock {
 public:
  explicit TestAutofillClock(base::Time now = {});
  explicit TestAutofillClock(std::unique_ptr<base::SimpleTestClock> test_clock);

  TestAutofillClock(const TestAutofillClock&) = delete;
  TestAutofillClock& operator=(const TestAutofillClock&) = delete;

  ~TestAutofillClock();

  // Set the time to be returned from AutofillClock::Now() calls.
  void SetNow(base::Time now);

  // Advances the clock by |delta|.
  void Advance(base::TimeDelta delta);

 private:
  std::unique_ptr<base::SimpleTestClock> test_clock_;
  raw_ptr<const base::Clock> previous_clock_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLOCK_H_
