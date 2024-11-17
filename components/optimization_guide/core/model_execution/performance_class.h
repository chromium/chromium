// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_PERFORMANCE_CLASS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_PERFORMANCE_CLASS_H_

#include "base/component_export.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/prefs/pref_service.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom-shared.h"

namespace optimization_guide {

// Convert from mojo enum.
OnDeviceModelPerformanceClass ConvertToOnDeviceModelPerformanceClass(
    on_device_model::mojom::PerformanceClass performance_class);

// Stores the performance class in the preferences file.
void UpdatePerformanceClassPref(
    PrefService* local_state,
    OnDeviceModelPerformanceClass performance_class);

// Loads the performance class from the preferences file.
OnDeviceModelPerformanceClass PerformanceClassFromPref(
    const PrefService& local_state);

// Check if the performance class is in the allowlist.
bool IsPerformanceClassCompatible(
    std::string perf_classes_string,
    OnDeviceModelPerformanceClass performance_class);

// Get the name of the synthetic trial group for this performance class.
std::string SyntheticTrialGroupForPerformanceClass(
    OnDeviceModelPerformanceClass performance_class);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_PERFORMANCE_CLASS_H_
