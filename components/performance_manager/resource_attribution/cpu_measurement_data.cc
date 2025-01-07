// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/cpu_measurement_data.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"
#include "components/performance_manager/resource_attribution/node_data_describers.h"

namespace resource_attribution {

CPUMeasurementData::CPUMeasurementData(
    std::unique_ptr<CPUMeasurementDelegate> delegate)
    : delegate_(std::move(delegate)),
      // Record the CPU usage immediately on starting to measure a process, so
      // the first call to CPUMeasurementMonitor::MeasureAndDistributeCPUUsage()
      // will cover the time between the measurement starting and the snapshot.
      most_recent_measurement_(
          base::OptionalFromExpected(delegate_->GetCumulativeCPUUsage())),
      last_measurement_time_(base::TimeTicks::Now()) {}

CPUMeasurementData::~CPUMeasurementData() = default;

CPUMeasurementData::CPUMeasurementData(CPUMeasurementData&&) = default;

CPUMeasurementData& CPUMeasurementData::operator=(CPUMeasurementData&&) =
    default;

void CPUMeasurementData::SetMostRecentMeasurement(
    base::TimeDelta measurement,
    base::TimeTicks measurement_time) {
  most_recent_measurement_ = measurement;
  last_measurement_time_ = measurement_time;
}

SharedCPUTimeResultData::SharedCPUTimeResultData() = default;

SharedCPUTimeResultData::~SharedCPUTimeResultData() = default;

SharedCPUTimeResultData::SharedCPUTimeResultData(SharedCPUTimeResultData&&) =
    default;

SharedCPUTimeResultData& SharedCPUTimeResultData::operator=(
    SharedCPUTimeResultData&&) = default;

base::Value::Dict SharedCPUTimeResultData::Describe() const {
  base::Value::Dict dict;
  if (result_ptr) {
    const CPUTimeResult& result = result_ptr->result();
    const base::TimeDelta measurement_interval =
        result.metadata.measurement_time - result.start_time;
    dict.Merge(DescribeResultMetadata(result.metadata));
    dict.Set("measurement_interval",
             performance_manager::TimeDeltaToValue(measurement_interval));
    dict.Set("cumulative_cpu",
             performance_manager::TimeDeltaToValue(result.cumulative_cpu));
    dict.Set("cumulative_background_cpu",
             performance_manager::TimeDeltaToValue(
                 result.cumulative_background_cpu));
  }
  return dict;
}

}  // namespace resource_attribution
