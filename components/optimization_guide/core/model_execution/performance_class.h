// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_PERFORMANCE_CLASS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_PERFORMANCE_CLASS_H_

#include "base/callback_list.h"
#include "base/component_export.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/prefs/pref_service.h"
#include "services/on_device_model/public/cpp/service_client.h"
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

// Computes performance class at most once, and allows observation of it's
// availability.
class PerformanceClassifier final {
 public:
  PerformanceClassifier(
      base::SafeRef<OnDeviceModelComponentStateManager>
          on_device_component_state_manager,
      base::SafeRef<on_device_model::ServiceClient> service_client);
  ~PerformanceClassifier();

  base::SafeRef<PerformanceClassifier> GetSafeRef() {
    return weak_ptr_factory_.GetSafeRef();
  }
  base::WeakPtr<PerformanceClassifier> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Ensures the performance class will be up to date and available when
  // `complete` runs.
  void EnsurePerformanceClassAvailable(base::OnceClosure complete);

  // Registers a callback to be called once performance class is available,
  // but does not trigger the computation. Returns true if it was already
  // available.
  bool ListenForPerformanceClassAvailable(base::OnceClosure available);

 private:
  // Called when performance class has finished updating.
  void PerformanceClassUpdated(OnDeviceModelPerformanceClass perf_class);

  // Notify observers that the performance class is available.
  void NotifyPerformanceClassAvailable();

  base::SafeRef<OnDeviceModelComponentStateManager>
      on_device_component_state_manager_;

  base::SafeRef<on_device_model::ServiceClient> service_client_;

  enum class PerformanceClassState {
    kNotSet,
    kComputing,
    kComplete,
  };
  PerformanceClassState performance_class_state_ =
      PerformanceClassState::kNotSet;

  // Callbacks waiting for performance class to finish computing.
  base::OnceClosureList performance_class_callbacks_;

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<PerformanceClassifier> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_PERFORMANCE_CLASS_H_
