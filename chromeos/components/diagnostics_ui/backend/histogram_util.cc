// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/histogram_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace chromeos {
namespace diagnostics {
namespace metrics {

void EmitAppOpenDuration(const base::TimeDelta& time_elapsed) {
  base::UmaHistogramLongTimes100("ChromeOS.DiagnosticsUi.OpenDuration",
                                 time_elapsed);
}

void EmitMemoryRoutineDuration(const base::TimeDelta& memory_routine_duration) {
  base::UmaHistogramLongTimes100("ChromeOS.DiagnosticsUi.MemoryRoutineDuration",
                                 memory_routine_duration);
}

void EmitRoutineRunCount(uint16_t routine_count) {
  base::UmaHistogramCounts100("ChromeOS.DiagnosticsUi.RoutineCount",
                              routine_count);
}

void EmitRoutineResult(mojom::RoutineType routine_type,
                       mojom::StandardRoutineResult result) {
  switch (routine_type) {
    case mojom::RoutineType::kBatteryCharge:
      base::UmaHistogramEnumeration(
          "ChromeOS.DiagnosticsUi.BatteryChargeResult", result);
      return;
    case mojom::RoutineType::kBatteryDischarge:
      base::UmaHistogramEnumeration(
          "ChromeOS.DiagnosticsUi.BatteryDischargeResult", result);
      return;
    case mojom::RoutineType::kCpuCache:
      base::UmaHistogramEnumeration("ChromeOS.DiagnosticsUi.CpuCacheResult",
                                    result);
      return;
    case mojom::RoutineType::kCpuFloatingPoint:
      base::UmaHistogramEnumeration(
          "ChromeOS.DiagnosticsUi.CpuFloatingPointResult", result);
      return;
    case mojom::RoutineType::kCpuPrime:
      base::UmaHistogramEnumeration("ChromeOS.DiagnosticsUi.CpuPrimeResult",
                                    result);
      return;
    case mojom::RoutineType::kCpuStress:
      base::UmaHistogramEnumeration("ChromeOS.DiagnosticsUi.CpuStressResult",
                                    result);
      return;
    case mojom::RoutineType::kMemory:
      base::UmaHistogramEnumeration("ChromeOS.DiagnosticsUi.MemoryResult",
                                    result);
      return;
  }
}

}  // namespace metrics
}  // namespace diagnostics
}  // namespace chromeos
