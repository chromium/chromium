// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLOCK_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLOCK_H_

#include <memory>

#include "base/macros.h"
#include "base/test/simple_test_clock.h"

namespace base {
class Time;
}  // namespace base

namespace autofill {

// Handles the customization of the time in tests. Replaces the clock in
// AutofillClock with a test version that can be manipulated from this class.
// Automatically resets a normal clock to AutofillClock when this gets
// destroyed.
class TestAutofillClock {
 public:
  TestAutofillClock();
  ~TestAutofillClock();

  // Set the time to be returned from AutofillClock::Now() calls.
  void SetNow(base::Time now);

  // Advances the clock by |delta|.
  void Advance(base::TimeDelta delta);

 private:
  base::SimpleTestClock test_clock_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillClock);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLOCK_H_
