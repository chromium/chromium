// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_metrics/mach_time_mac.h"

#include "base/apple/mach_logging.h"
#include "base/check.h"

namespace power_metrics {

uint64_t MachTimeToNs(uint64_t mach_time,
                      const mach_timebase_info_data_t& mach_timebase) {
  if (mach_timebase.numer == mach_timebase.denom)
    return mach_time;

  CHECK(!__builtin_umulll_overflow(mach_time, mach_timebase.numer, &mach_time));
  return mach_time / mach_timebase.denom;
}

mach_timebase_info_data_t GetSystemMachTimeBase() {
  mach_timebase_info_data_t info;
  kern_return_t kr = mach_timebase_info(&info);
  MACH_DCHECK(kr == KERN_SUCCESS, kr) << "mach_timebase_info";
  DCHECK(info.numer);
  DCHECK(info.denom);
  return info;
}

}  // namespace power_metrics
