// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_METRICS_MACH_TIME_MAC_H_
#define COMPONENTS_POWER_METRICS_MACH_TIME_MAC_H_

#include <mach/mach_time.h>
#include <stdint.h>

namespace power_metrics {

// Converts |mach_time| to nanoseconds, using the multiplier in |mach_timebase|.
uint64_t MachTimeToNs(uint64_t mach_time,
                      const mach_timebase_info_data_t& mach_timebase);

// Retrieves the |mach_timebase| to convert |mach_time| obtained on this system
// to nanoseconds.
mach_timebase_info_data_t GetSystemMachTimeBase();

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_MACH_TIME_MAC_H_
