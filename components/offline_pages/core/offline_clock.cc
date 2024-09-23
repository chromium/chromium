// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_clock.h"

#include <ostream>

#include "base/check.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"

namespace offline_pages {

namespace {
const base::Clock* custom_clock_ = nullptr;
}

const base::Clock* OfflineClock() {
  if (custom_clock_) {
    return custom_clock_;
  }
  return base::DefaultClock::GetInstance();
}

void SetOfflineClockForTesting(const base::Clock* clock) {
  DCHECK(clock == nullptr || custom_clock_ == nullptr)
      << "Offline clock is being overridden a second time, which might "
         "indicate a bug.";
  custom_clock_ = clock;
}

base::Time OfflineTimeNow() {
  return OfflineClock()->Now();
}

}  // namespace offline_pages
