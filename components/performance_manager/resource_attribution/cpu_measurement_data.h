// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_DATA_H_

#include <memory>
#include <optional>

#include "base/time/time.h"
#include "components/performance_manager/graph/node_inline_data.h"

namespace resource_attribution {

class CPUMeasurementDelegate;

// Holds a CPUMeasurementDelegate object to measure CPU usage and metadata
// about the measurements. A CPUMeasurementData will be created by
// CPUMeasurementMonitor for each ProcessNode being measured.
class CPUMeasurementData
    : public performance_manager::SparseNodeInlineData<CPUMeasurementData> {
 public:
  explicit CPUMeasurementData(std::unique_ptr<CPUMeasurementDelegate> delegate);
  ~CPUMeasurementData();

  // Move-only type.
  CPUMeasurementData(const CPUMeasurementData& other) = delete;
  CPUMeasurementData& operator=(const CPUMeasurementData& other) = delete;
  CPUMeasurementData(CPUMeasurementData&& other);
  CPUMeasurementData& operator=(CPUMeasurementData&& other);

  CPUMeasurementDelegate& measurement_delegate() { return *delegate_; }

  std::optional<base::TimeDelta> most_recent_measurement() const {
    return most_recent_measurement_;
  }

  base::TimeTicks last_measurement_time() const {
    return last_measurement_time_;
  }

  void SetMostRecentMeasurement(base::TimeDelta measurement,
                                base::TimeTicks measurement_time);

 private:
  std::unique_ptr<CPUMeasurementDelegate> delegate_;
  std::optional<base::TimeDelta> most_recent_measurement_;
  base::TimeTicks last_measurement_time_;
};

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_CPU_MEASUREMENT_DATA_H_
