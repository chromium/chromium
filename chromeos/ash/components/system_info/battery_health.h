// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_BATTERY_HEALTH_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_BATTERY_HEALTH_H_

#include <string>

#include "base/component_export.h"

namespace system_info {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO) BatteryHealth {
 public:
  BatteryHealth() = default;

  ~BatteryHealth() = default;

  void SetCycleCount(int cycle_count) { cycle_count_ = cycle_count; }
  void SetBatteryWearPercentage(int battery_wear_percentage) {
    battery_wear_percentage_ = battery_wear_percentage;
  }
  void SetPowerTime(const std::u16string& power_time) {
    power_time_ = power_time;
  }
  void SetAccessibilityLabel(const std::u16string& accessibility_label) {
    accessibility_label_ = accessibility_label;
  }
  void SetBatteryPercentage(int battery_percentage) {
    battery_percentage_ = battery_percentage;
  }

  int GetCycleCount() const { return cycle_count_; }
  int GetBatteryWearPercentage() const { return battery_wear_percentage_; }
  std::u16string GetPowerTime() const { return power_time_; }
  std::u16string GetAccessibilityLabel() const { return accessibility_label_; }
  int GetBatteryPercentage() const { return battery_percentage_; }

 private:
  int cycle_count_ = 0;
  int battery_wear_percentage_ = 0;
  std::u16string power_time_ = u"";
  std::u16string accessibility_label_ = u"";
  int battery_percentage_ = 0;
};

}  // namespace system_info

#endif  // CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_BATTERY_HEALTH_H_
