// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_tick_clock.h"

#include <utility>

#include "base/test/simple_test_tick_clock.h"
#include "components/autofill/core/common/autofill_tick_clock.h"

namespace autofill {

TestAutofillTickClock::TestAutofillTickClock(base::TimeTicks now_ticks) {
  AutofillTickClock::SetTestTickClock(&test_tick_clock_);
  SetNowTicks(now_ticks);
}

TestAutofillTickClock::~TestAutofillTickClock() {
  // Destroys the test tick_clock and resets a normal tick_clock.
  AutofillTickClock::SetTickClock();
}

void TestAutofillTickClock::SetNowTicks(base::TimeTicks now) {
  test_tick_clock_.SetNowTicks(now);
}

void TestAutofillTickClock::Advance(base::TimeDelta delta) {
  test_tick_clock_.Advance(delta);
}

}  // namespace autofill
