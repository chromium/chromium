// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/performance_class.h"

#include "base/containers/contains.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/version_info/version_info.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/variations/synthetic_trials.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace optimization_guide {

OnDeviceModelPerformanceClass ConvertToOnDeviceModelPerformanceClass(
    on_device_model::mojom::PerformanceClass performance_class) {
  switch (performance_class) {
    case on_device_model::mojom::PerformanceClass::kError:
      return OnDeviceModelPerformanceClass::kError;
    case on_device_model::mojom::PerformanceClass::kVeryLow:
      return OnDeviceModelPerformanceClass::kVeryLow;
    case on_device_model::mojom::PerformanceClass::kLow:
      return OnDeviceModelPerformanceClass::kLow;
    case on_device_model::mojom::PerformanceClass::kMedium:
      return OnDeviceModelPerformanceClass::kMedium;
    case on_device_model::mojom::PerformanceClass::kHigh:
      return OnDeviceModelPerformanceClass::kHigh;
    case on_device_model::mojom::PerformanceClass::kVeryHigh:
      return OnDeviceModelPerformanceClass::kVeryHigh;
    case on_device_model::mojom::PerformanceClass::kGpuBlocked:
      return OnDeviceModelPerformanceClass::kGpuBlocked;
    case on_device_model::mojom::PerformanceClass::kFailedToLoadLibrary:
      return OnDeviceModelPerformanceClass::kFailedToLoadLibrary;
  }
}

std::string SyntheticTrialGroupForPerformanceClass(
    OnDeviceModelPerformanceClass performance_class) {
  switch (performance_class) {
    case OnDeviceModelPerformanceClass::kUnknown:
      return "Unknown";
    case OnDeviceModelPerformanceClass::kError:
      return "Error";
    case OnDeviceModelPerformanceClass::kVeryLow:
      return "VeryLow";
    case OnDeviceModelPerformanceClass::kLow:
      return "Low";
    case OnDeviceModelPerformanceClass::kMedium:
      return "Medium";
    case OnDeviceModelPerformanceClass::kHigh:
      return "High";
    case OnDeviceModelPerformanceClass::kVeryHigh:
      return "VeryHigh";
    case OnDeviceModelPerformanceClass::kGpuBlocked:
      return "GpuBlocked";
    case OnDeviceModelPerformanceClass::kFailedToLoadLibrary:
      return "FailedToLoadLibrary";
    case OnDeviceModelPerformanceClass::kServiceCrash:
      return "ServiceCrash";
  }
}

bool IsPerformanceClassCompatible(
    std::string perf_classes_string,
    OnDeviceModelPerformanceClass performance_class) {
  if (perf_classes_string == "*") {
    return true;
  }
  std::vector<std::string_view> perf_classes_list = base::SplitStringPiece(
      perf_classes_string, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  return base::Contains(perf_classes_list,
                        base::ToString(static_cast<int>(performance_class)));
}

OnDeviceModelPerformanceClass PerformanceClassFromPref(
    const PrefService& local_state) {
  int value = local_state.GetInteger(
      model_execution::prefs::localstate::kOnDevicePerformanceClass);
  if (value < 0 ||
      value > static_cast<int>(OnDeviceModelPerformanceClass::kMaxValue)) {
    return OnDeviceModelPerformanceClass::kUnknown;
  }
  return static_cast<OnDeviceModelPerformanceClass>(value);
}

void UpdatePerformanceClassPref(
    PrefService* local_state,
    OnDeviceModelPerformanceClass performance_class) {
  local_state->SetInteger(
      model_execution::prefs::localstate::kOnDevicePerformanceClass,
      base::to_underlying(performance_class));
  local_state->SetString(
      model_execution::prefs::localstate::kOnDevicePerformanceClassVersion,
      version_info::GetVersionNumber());
}

PerformanceClassifier::PerformanceClassifier(
    base::SafeRef<OnDeviceModelComponentStateManager>
        on_device_component_state_manager,
    base::SafeRef<on_device_model::ServiceClient> service_client)
    : on_device_component_state_manager_(
          std::move(on_device_component_state_manager)),
      service_client_(std::move(service_client)) {}
PerformanceClassifier::~PerformanceClassifier() = default;

void PerformanceClassifier::EnsurePerformanceClassAvailable(
    base::OnceClosure complete) {
  if (!features::CanLaunchOnDeviceModelService()) {
    std::move(complete).Run();
    return;
  }

  if (ListenForPerformanceClassAvailable(std::move(complete))) {
    return;
  }

  if (performance_class_state_ == PerformanceClassState::kComputing) {
    return;
  }

  performance_class_state_ = PerformanceClassState::kComputing;
  service_client_->Get()->GetDevicePerformanceInfo(
      base::BindOnce([](on_device_model::mojom::DevicePerformanceInfoPtr info) {
        return info->performance_class;
      })
          .Then(base::BindOnce(&ConvertToOnDeviceModelPerformanceClass)
                    .Then(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                        base::BindOnce(
                            &PerformanceClassifier::PerformanceClassUpdated,
                            weak_ptr_factory_.GetWeakPtr()),
                        OnDeviceModelPerformanceClass::kServiceCrash))));
}

bool PerformanceClassifier::ListenForPerformanceClassAvailable(
    base::OnceClosure available) {
  if (!on_device_component_state_manager_->NeedsPerformanceClassUpdate()) {
    std::move(available).Run();
    return true;
  }

  if (performance_class_state_ == PerformanceClassState::kComplete) {
    std::move(available).Run();
    return true;
  }

  // Use unsafe because cancellation isn't needed.
  performance_class_callbacks_.AddUnsafe(std::move(available));
  return false;
}

void PerformanceClassifier::PerformanceClassUpdated(
    OnDeviceModelPerformanceClass perf_class) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution.OnDeviceModelPerformanceClass",
      perf_class);

  auto complete =
      base::BindOnce(&PerformanceClassifier::NotifyPerformanceClassAvailable,
                     weak_ptr_factory_.GetWeakPtr());
  on_device_component_state_manager_->DevicePerformanceClassChanged(
      std::move(complete), perf_class);
}

void PerformanceClassifier::NotifyPerformanceClassAvailable() {
  performance_class_state_ = PerformanceClassState::kComplete;
  performance_class_callbacks_.Notify();
}

}  // namespace optimization_guide
