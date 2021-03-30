// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_HISTOGRAM_UTIL_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_HISTOGRAM_UTIL_H_

#include <cstdint>

#include "chromeos/components/diagnostics_ui/mojom/system_routine_controller.mojom.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace chromeos {
namespace diagnostics {
namespace metrics {

void EmitAppOpenDuration(const base::TimeDelta& time_elapsed);

void EmitMemoryRoutineDuration(const base::TimeDelta& memory_routine_duration);

void EmitRoutineRunCount(uint16_t routine_count);

void EmitRoutineResult(mojom::RoutineType routine_type,
                       mojom::StandardRoutineResult result);

}  // namespace metrics
}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_HISTOGRAM_UTIL_H_
