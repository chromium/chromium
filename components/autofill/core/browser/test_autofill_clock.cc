// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_clock.h"

#include <memory>
#include <utility>

#include "base/test/simple_test_clock.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

TestAutofillClock::TestAutofillClock(base::Time now)
    : TestAutofillClock(std::make_unique<base::SimpleTestClock>()) {
  SetNow(now);
}

TestAutofillClock::TestAutofillClock(
    std::unique_ptr<base::SimpleTestClock> test_clock)
    : test_clock_(std::move(test_clock)) {
  AutofillClock::SetTestClock(test_clock_.get());
}

TestAutofillClock::~TestAutofillClock() {
  // Destroys the test clock and resets a normal clock.
  AutofillClock::SetClock();
}

void TestAutofillClock::SetNow(base::Time now) {
  test_clock_->SetNow(now);
}

void TestAutofillClock::Advance(base::TimeDelta delta) {
  test_clock_->Advance(delta);
}

}  // namespace autofill
