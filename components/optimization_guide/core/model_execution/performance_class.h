// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_PERFORMANCE_CLASS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_PERFORMANCE_CLASS_H_

#include "base/callback_list.h"
#include "base/component_export.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/proto/on_device_base_model_metadata.pb.h"
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

// Stores the device info in the preferences file.
void UpdateDeviceInfoPrefs(PrefService* local_state,
                           uint32_t vendor_id,
                           uint32_t device_id,
                           std::string driver_version,
                           bool supports_fp16);

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

// Get the name of the synthetic trial group for this performance hint.
std::string SyntheticTrialGroupForPerformanceHint(
    proto::OnDeviceModelPerformanceHint performance_hint);

// Computes performance class at most once, and allows observation of it's
// availability.
class PerformanceClassifier final {
 public:
  PerformanceClassifier(
      PrefService* local_state,
      base::SafeRef<on_device_model::ServiceClient> service_client);
  ~PerformanceClassifier();

  base::SafeRef<PerformanceClassifier> GetSafeRef() {
    return weak_ptr_factory_.GetSafeRef();
  }

  // Ensures the performance class will be up to date and available when
  // `complete` runs.
  void EnsurePerformanceClassAvailable(base::OnceClosure complete);

  // Registers a callback to be called once performance class is available,
  // but does not trigger the computation. Returns true if it was already
  // available.
  bool ListenForPerformanceClassAvailable(base::OnceClosure available);

  // Schedules a call to EnsurePerformanceClassAvailable after a delay.
  void ScheduleEvaluation();

  // Whether the performance class has been determined yet.
  bool IsPerformanceClassAvailable() const {
    return performance_class_state_ == PerformanceClassState::kComplete;
  }

  // The below methods all require PerformanceClassAvailable and may give a
  // stale value otherwise.

  // Returns the latest performance class.
  OnDeviceModelPerformanceClass GetPerformanceClass() const;
  // Returns true if this device can run any foundational model.
  bool IsDeviceCapable() const;
  // Returns true if this device can run any GPU foundational model.
  bool IsDeviceGPUCapable() const;
  // Returns true if this is determined to be a low tier device.
  bool IsLowTierDevice() const;
  // Returns true if the device supports image input.
  bool SupportsImageInput() const;
  // Returns true if the device supports audio input.
  bool SupportsAudioInput() const;
  // Returns a list of performance hints this device supports in priority order,
  // with highest priority first.
  std::vector<proto::OnDeviceModelPerformanceHint> GetPossibleHints() const;

  on_device_model::Capabilities GetPossibleOnDeviceCapabilities() const;

 private:
  // Called when performance class has finished evaluating.
  void OnDeviceAndPerformanceInfo(
      on_device_model::mojom::DevicePerformanceInfoPtr perf_info,
      on_device_model::mojom::DeviceInfoPtr device_info);

  raw_ptr<PrefService> local_state_;

  base::SafeRef<on_device_model::ServiceClient> service_client_;

  enum class PerformanceClassState {
    kNotStarted,
    kComputing,
    kComplete,
  };
  PerformanceClassState performance_class_state_ =
      PerformanceClassState::kNotStarted;

  // Callbacks waiting for performance class to finish computing.
  base::OnceClosureList performance_class_callbacks_;

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<PerformanceClassifier> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_PERFORMANCE_CLASS_H_
