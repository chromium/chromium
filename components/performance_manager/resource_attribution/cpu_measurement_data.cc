// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/cpu_measurement_data.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"

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

CPUMeasurementData::CPUMeasurementData(CPUMeasurementData&& other) = default;

CPUMeasurementData& CPUMeasurementData::operator=(CPUMeasurementData&& other) =
    default;

void CPUMeasurementData::SetMostRecentMeasurement(
    base::TimeDelta measurement,
    base::TimeTicks measurement_time) {
  most_recent_measurement_ = measurement;
  last_measurement_time_ = measurement_time;
}

}  // namespace resource_attribution
