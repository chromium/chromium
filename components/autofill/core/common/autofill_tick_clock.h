// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_TICK_CLOCK_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_TICK_CLOCK_H_

namespace base {
class TickClock;
class TimeTicks;
}  // namespace base

namespace autofill {

// Handles getting the current time for the Autofill feature. Can be injected
// with a customizable tick clock to facilitate testing.
class AutofillTickClock {
 public:
  // Returns the current time based last set tick clock.
  static base::TimeTicks NowTicks();

 private:
  friend class TestAutofillTickClock;

  // Resets a normal tick clock.
  static void SetTickClock();

  // Sets the tick clock to be used for tests.
  static void SetTestTickClock(const base::TickClock* tick_clock);

  AutofillTickClock() = delete;
  ~AutofillTickClock() = delete;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_TICK_CLOCK_H_
