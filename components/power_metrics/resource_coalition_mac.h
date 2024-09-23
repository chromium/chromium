// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Resource Coalition is an (undocumented) mechanism available in macOS that
// allows retrieving accrued resource usage metrics for a group of processes,
// including processes that have died. Typically, a coalition includes a root
// process and its descendants. The source can help understand the mechanism:
// https://github.com/apple/darwin-xnu/blob/main/osfmk/kern/coalition.c

#ifndef COMPONENTS_POWER_METRICS_RESOURCE_COALITION_MAC_H_
#define COMPONENTS_POWER_METRICS_RESOURCE_COALITION_MAC_H_

#include <mach/mach_time.h>
#include <stdint.h>

#include <memory>
#include <optional>

#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "components/power_metrics/resource_coalition_internal_types_mac.h"

namespace power_metrics {

struct EnergyImpactCoefficients;

// Returns the coalition id for the process identified by |pid| or nullopt if
// not available.
std::optional<uint64_t> GetProcessCoalitionId(base::ProcessId pid);

// Returns resource usage data for the coalition identified by |coalition_id|,
// or nullptr if not available (e.g. if `coalition_id` is invalid or if the
// kernel can't allocate memory).
std::unique_ptr<coalition_resource_usage> GetCoalitionResourceUsage(
    int64_t coalition_id);

// Returns a `coalition_resource_usage` in which each member is the result of
// subtracting the corresponding fields in `left` and `right`.
coalition_resource_usage GetCoalitionResourceUsageDifference(
    const coalition_resource_usage& left,
    const coalition_resource_usage& right);

// Struct that contains the rate of resource usage for a coalition.
struct CoalitionResourceUsageRate {
  double cpu_time_per_second;
  double interrupt_wakeups_per_second;
  double platform_idle_wakeups_per_second;
  double bytesread_per_second;
  double byteswritten_per_second;
  double gpu_time_per_second;
  // Only makes sense on Intel macs, not computed on M1 macs.
  std::optional<double> energy_impact_per_second;
  // Only available on M1 macs as of September 2021.
  double power_nw;

  double qos_time_per_second[THREAD_QOS_LAST];
};

// Returns rate of resource usage for a coalition, given the usage at
// the beginning and end of an interval and the duration of the interval.
std::optional<CoalitionResourceUsageRate> GetCoalitionResourceUsageRate(
    const coalition_resource_usage& begin,
    const coalition_resource_usage& end,
    base::TimeDelta interval_duration,
    mach_timebase_info_data_t timebase,
    std::optional<EnergyImpactCoefficients> energy_impact_coefficients);

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_RESOURCE_COALITION_MAC_H_
