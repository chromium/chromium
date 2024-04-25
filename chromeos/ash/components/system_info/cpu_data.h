// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_CPU_DATA_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_CPU_DATA_H_

#include <string>

#include "base/component_export.h"
#include "base/strings/string_number_conversions.h"

namespace system_info {

/* This class is used to store the final CPU usage and health data for the
System Info Provider. */
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO) CpuData {
 public:
  CpuData() = default;

  ~CpuData() = default;

  void SetPercentUsageUser(int percent_usage_user) {
    percent_usage_user_ = percent_usage_user;
  }
  void SetPercentUsageSystem(int percent_usage_system) {
    percent_usage_system_ = percent_usage_system;
  }
  void SetPercentUsageFree(int percent_usage_free) {
    percent_usage_free_ = percent_usage_free;
  }
  void SetAverageCpuTempCelsius(int average_cpu_temp_celsius) {
    average_cpu_temp_celsius_ = average_cpu_temp_celsius;
  }
  void SetScalingAverageCurrentFrequencyKhz(int scaling_current_frequency_khz) {
    scaling_current_frequency_khz_ = scaling_current_frequency_khz;
  }

  int GetPercentUsageUser() const { return percent_usage_user_; }
  int GetPercentUsageSystem() const { return percent_usage_system_; }
  int GetPercentUsageFree() const { return percent_usage_free_; }
  std::u16string GetPercentUsageTotalString() const {
    return base::NumberToString16(percent_usage_user_ + percent_usage_system_);
  }
  int GetAverageCpuTempCelsius() const { return average_cpu_temp_celsius_; }
  int GetScalingAverageCurrentFrequencyKhz() const {
    return scaling_current_frequency_khz_;
  }

 private:
  int percent_usage_user_ = 0;
  int percent_usage_system_ = 0;
  int percent_usage_free_ = 0;
  int average_cpu_temp_celsius_ = 0;
  int scaling_current_frequency_khz_ = 0;
};

}  // namespace system_info

#endif  // CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_CPU_DATA_H_
