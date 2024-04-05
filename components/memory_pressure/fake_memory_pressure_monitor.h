// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_FAKE_MEMORY_PRESSURE_MONITOR_H_
#define COMPONENTS_MEMORY_PRESSURE_FAKE_MEMORY_PRESSURE_MONITOR_H_

#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"

namespace memory_pressure {
namespace test {

class FakeMemoryPressureMonitor
    : public ::memory_pressure::MultiSourceMemoryPressureMonitor {
 public:
  using MemoryPressureLevel =
      ::memory_pressure::MultiSourceMemoryPressureMonitor::MemoryPressureLevel;
  using DispatchCallback =
      ::memory_pressure::MultiSourceMemoryPressureMonitor::DispatchCallback;

  FakeMemoryPressureMonitor();
  ~FakeMemoryPressureMonitor() override;

  FakeMemoryPressureMonitor(const FakeMemoryPressureMonitor&) = delete;
  FakeMemoryPressureMonitor& operator=(const FakeMemoryPressureMonitor&) =
      delete;

  void SetAndNotifyMemoryPressure(MemoryPressureLevel level);

  // base::MemoryPressureMonitor overrides:
  MemoryPressureLevel GetCurrentPressureLevel() const override;

 private:
  MemoryPressureLevel memory_pressure_level_{
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_NONE};
};

}  // namespace test
}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_FAKE_MEMORY_PRESSURE_MONITOR_H_
