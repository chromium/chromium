// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_TICK_CLOCK_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_TICK_CLOCK_H_

#include <memory>

#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace autofill {

// Handles the customization of the time in tests. Replaces the tick_clock in
// AutofillTickClock with a test version that can be manipulated from this
// class. Automatically resets a normal tick_clock to AutofillTickClock when
// this gets destroyed.
class TestAutofillTickClock {
 public:
  TestAutofillTickClock();
  ~TestAutofillTickClock();

  // Set the time to be returned from AutofillTickClock::Now() calls.
  void SetNowTicks(base::TimeTicks now);

  // Advances the tick_clock by |delta|.
  void Advance(base::TimeDelta delta);

 private:
  base::SimpleTestTickClock test_tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillTickClock);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_TICK_CLOCK_H_
