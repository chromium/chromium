// Copyright 2017 The Chromium Authors
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
// TODO crbug.com/40100455 - Remove this class.
class AutofillClock {
 public:
  AutofillClock() = delete;
  ~AutofillClock() = delete;

  // Returns the current time based last set clock.
  // This is thread-safe in production code.
  // In test code, there may be races among SetTestClock() and Now().
  static base::Time Now();

 private:
  friend class TestAutofillClock;

  static const base::Clock* test_clock_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CLOCK_H_
