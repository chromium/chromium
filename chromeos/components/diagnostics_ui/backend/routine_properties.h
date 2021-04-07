// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_ROUTINE_PROPERTIES_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_ROUTINE_PROPERTIES_H_

#include <stddef.h>
#include <cstdint>
#include <string>

#include "chromeos/components/diagnostics_ui/mojom/system_routine_controller.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"

namespace chromeos {
namespace diagnostics {

// Contains information related to a mojom::RoutineType, used in conjunction
// with the kRoutineProperties array to provide access to information
// about a given mojom::RoutineType.
struct RoutineProperties {
  mojom::RoutineType type;
  const char* metric_name;
  uint32_t duration_seconds;
  cros_healthd::mojom::DiagnosticRoutineEnum healthd_type;
};

extern const RoutineProperties kRoutineProperties[];
extern const size_t kRoutinePropertiesLength;

// Returns the metric name for a given routine type.
std::string GetRoutineMetricName(mojom::RoutineType routine_type);

// Returns the expected routine duration in seconds for a given routine type.
uint32_t GetExpectedRoutineDurationInSeconds(mojom::RoutineType routine_type);

// Helper function that casts a mojom::RoutineType to a number and returns
// the corresponding RoutineProperties struct.
const RoutineProperties& GetRoutineProperties(mojom::RoutineType routine_type);

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_ROUTINE_PROPERTIES_H_