// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_CPU_TEMPERATURE_READER_H_
#define CHROMEOS_SYSTEM_CPU_TEMPERATURE_READER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"

namespace chromeos {
namespace system {

// Used to read CPU temperature info from sysfs hwmon.
class COMPONENT_EXPORT(CHROMEOS_SYSTEM) CPUTemperatureReader {
 public:
  // Contains info from a CPU temperature sensor.
  struct CPUTemperatureInfo {
    CPUTemperatureInfo();
    ~CPUTemperatureInfo();

    bool operator<(const CPUTemperatureInfo& other) const {
      return std::make_pair(label, temp_celsius) <
             std::make_pair(other.label, other.temp_celsius);
    }

    // The temperature read by a CPU temperature sensor in degrees Celsius.
    double temp_celsius;

    // The name of the CPU temperature zone monitored by this sensor. Used to
    // identify the source of each temperature reading. Taken from sysfs "name"
    // or "label" field, if it exists.
    std::string label;
  };

  CPUTemperatureReader();
  ~CPUTemperatureReader();

  void set_hwmon_dir_for_test(const base::FilePath& dir) { hwmon_dir_ = dir; }

  // Reads temperature from each thermal sensor of the CPU. Returns a vector
  // containing a reading from each sensor. This is a blocking function that
  // should be run on a thread that allows blocking operations.
  std::vector<CPUTemperatureInfo> GetCPUTemperatures();

 private:
  // Sysfs hwmon directory path.
  base::FilePath hwmon_dir_;

  DISALLOW_COPY_AND_ASSIGN(CPUTemperatureReader);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_CPU_TEMPERATURE_READER_H_
