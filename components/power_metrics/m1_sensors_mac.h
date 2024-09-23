// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The Apple M1 chip has sensors to monitor its power consumption and
// temperature. This file defines a class to retrieve data from these sensors.

#ifndef COMPONENTS_POWER_METRICS_M1_SENSORS_MAC_H_
#define COMPONENTS_POWER_METRICS_M1_SENSORS_MAC_H_

#include <IOKit/hidsystem/IOHIDEventSystemClient.h>

#include <memory>
#include <optional>

#include "base/apple/scoped_cftyperef.h"

namespace power_metrics {

class M1SensorsReader {
 public:
  struct TemperaturesCelsius {
    TemperaturesCelsius();
    TemperaturesCelsius(const TemperaturesCelsius&) noexcept;
    ~TemperaturesCelsius();

    std::optional<double> p_cores;
    std::optional<double> e_cores;
  };

  virtual ~M1SensorsReader();

  // Creates an M1SensorsReader. Returns nullptr on failure.
  static std::unique_ptr<M1SensorsReader> Create();

  // Reads temperature sensors. Virtual for testing.
  virtual TemperaturesCelsius ReadTemperatures();

 protected:
  explicit M1SensorsReader(
      base::apple::ScopedCFTypeRef<IOHIDEventSystemClientRef> system);

 private:
  base::apple::ScopedCFTypeRef<IOHIDEventSystemClientRef> system_;
};

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_M1_SENSORS_MAC_H_
