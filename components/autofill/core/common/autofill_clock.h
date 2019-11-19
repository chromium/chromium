// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CLOCK_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CLOCK_H_

namespace base {
class Clock;
class Time;
}  // namespace base

namespace autofill {

// Handles getting the current time for the Autofill feature. Can be injected
// with a customizable clock to facilitate testing.
class AutofillClock {
 public:
  // Returns the current time based last set clock.
  static base::Time Now();

 private:
  friend class TestAutofillClock;

  // Resets a normal clock.
  static void SetClock();

  // Sets the clock to be used for tests.
  static void SetTestClock(const base::Clock* clock);

  AutofillClock() = delete;
  ~AutofillClock() = delete;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CLOCK_H_
