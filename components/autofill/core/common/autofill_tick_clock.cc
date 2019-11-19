// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_tick_clock.h"

#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"

namespace autofill {
namespace {
const base::TickClock* g_autofill_tick_clock = nullptr;
}  // namespace

// static
base::TimeTicks AutofillTickClock::NowTicks() {
  if (!g_autofill_tick_clock)
    SetTickClock();
  return g_autofill_tick_clock->NowTicks();
}

// static
void AutofillTickClock::SetTickClock() {
  g_autofill_tick_clock = base::DefaultTickClock::GetInstance();
}

// static
void AutofillTickClock::SetTestTickClock(const base::TickClock* tick_clock) {
  DCHECK(tick_clock);
  g_autofill_tick_clock = tick_clock;
}

}  // namespace autofill
