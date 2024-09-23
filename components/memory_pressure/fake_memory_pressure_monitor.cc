// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/fake_memory_pressure_monitor.h"

#include "base/logging.h"

namespace memory_pressure {
namespace test {

FakeMemoryPressureMonitor::FakeMemoryPressureMonitor() = default;

FakeMemoryPressureMonitor::~FakeMemoryPressureMonitor() = default;

void FakeMemoryPressureMonitor::SetAndNotifyMemoryPressure(
    MemoryPressureLevel level) {
  memory_pressure_level_ = level;
  base::MemoryPressureListener::SimulatePressureNotification(level);
}

base::MemoryPressureMonitor::MemoryPressureLevel
FakeMemoryPressureMonitor::GetCurrentPressureLevel() const {
  return memory_pressure_level_;
}

}  // namespace test
}  // namespace memory_pressure
