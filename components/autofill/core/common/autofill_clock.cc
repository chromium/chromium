// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_clock.h"

#include "base/time/clock.h"
#include "base/time/default_clock.h"

namespace autofill {
namespace {
const base::Clock* g_autofill_clock = nullptr;
}  // namespace

// static
base::Time AutofillClock::Now() {
  if (!g_autofill_clock)
    SetClock();
  return g_autofill_clock->Now();
}

// static
void AutofillClock::SetClock() {
  g_autofill_clock = base::DefaultClock::GetInstance();
}

// static
void AutofillClock::SetTestClock(const base::Clock* clock) {
  DCHECK(clock);
  g_autofill_clock = clock;
}

}  // namespace autofill
