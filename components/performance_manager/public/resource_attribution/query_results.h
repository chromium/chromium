// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERY_RESULTS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERY_RESULTS_H_

#include "base/time/time.h"

namespace performance_manager::resource_attribution {

// The Resource Attribution result and metadata structs described in
// https://bit.ly/resource-attribution-api#heading=h.k8fjwkwxxdj6.

// TODO(crbug.com/1471683): Add MeasurementAlgorithm to metadata

// Metadata about the measurement that produced a result.
struct ResultMetadata {
  // The time this measurement was taken.
  base::TimeTicks measurement_time;
};

// The result of a kCPUTime query.
struct CPUTimeResult {
  ResultMetadata metadata;

  // The time that Resource Attribution started monitoring the CPU usage of this
  // context.
  base::TimeTicks start_time;

  // Total time the context spent on CPU between `start_time` and
  // `metadata.measurement_time`.
  //
  // `cumulative_cpu` / (`metadata.measurement_time` - `start_time`)
  // gives percentage of CPU used as a fraction in the range 0% to 100% *
  // SysInfo::NumberOfProcessors(), the same as
  // ProcessMetrics::GetPlatformIndependentCPUUsage().
  base::TimeDelta cumulative_cpu;
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_QUERY_RESULTS_H_
