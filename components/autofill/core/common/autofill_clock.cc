// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_clock.h"

#include "base/check.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"

namespace autofill {

const base::Clock* AutofillClock::test_clock_ = nullptr;

// static
base::Time AutofillClock::Now() {
  return (test_clock_ ? test_clock_ : base::DefaultClock::GetInstance())->Now();
}

}  // namespace autofill
