// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"

#include "base/metrics/histogram_functions.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace optimization_guide {
namespace {

using model_execution::prefs::localstate::kOnDeviceModelChromeVersion;
using model_execution::prefs::localstate::kOnDeviceModelCrashCount;
using model_execution::prefs::localstate::kOnDeviceModelTimeoutCount;
using model_execution::prefs::localstate::kOnDeviceModelValidationResult;

const char kComponentVersionKey[] = "component_version";
const char kResultKey[] = "result";
const char kAttemptCountKey[] = "attempt_count";

OnDeviceModelValidationResult ConvertToOnDeviceModelValidationResult(
    int value) {
  if (value < 0 ||
      value > static_cast<int>(OnDeviceModelValidationResult::kMaxValue)) {
    return OnDeviceModelValidationResult::kUnknown;
  }
  return static_cast<OnDeviceModelValidationResult>(value);
}

}  // namespace

OnDeviceModelAccessController::OnDeviceModelAccessController(
    PrefService& pref_service)
    : pref_service_(pref_service) {
  if (pref_service_->GetString(kOnDeviceModelChromeVersion) !=
      version_info::GetVersionNumber()) {
    // When the version changes, reset the counts so that we try again.
    pref_service_->SetInteger(kOnDeviceModelCrashCount, 0);
    pref_service_->SetInteger(kOnDeviceModelTimeoutCount, 0);
    pref_service_->SetString(kOnDeviceModelChromeVersion,
                             version_info::GetVersionNumber());
    if (features::ShouldOnDeviceModelClearValidationOnVersionChange()) {
      pref_service_->SetDict(kOnDeviceModelValidationResult,
                             base::Value::Dict());
    } else {
      // If the full validation result is not cleared, at least reset the
      // attempt count to allow validation to continue.
      ValidationState state = GetValidationState();
      if (state.attempt_count > 0) {
        state.attempt_count = 0;
        SetValidationState(state);
      }
    }
  }
}

OnDeviceModelAccessController::~OnDeviceModelAccessController() = default;

OnDeviceModelEligibilityReason
OnDeviceModelAccessController::ShouldStartNewSession() const {
  if (is_gpu_blocked_) {
    return OnDeviceModelEligibilityReason::kGpuBlocked;
  }
  if (pref_service_->GetInteger(kOnDeviceModelCrashCount) >=
      features::GetOnDeviceModelCrashCountBeforeDisable()) {
    return OnDeviceModelEligibilityReason::kTooManyRecentCrashes;
  }
  if (pref_service_->GetInteger(kOnDeviceModelTimeoutCount) >=
      features::GetOnDeviceModelTimeoutCountBeforeDisable()) {
    return OnDeviceModelEligibilityReason::kTooManyRecentTimeouts;
  }
  if (features::IsOnDeviceModelValidationEnabled() &&
      features::ShouldOnDeviceModelBlockOnValidationFailure()) {
    ValidationState state = GetValidationState();
    if (state.component_version.empty() ||
        state.result == OnDeviceModelValidationResult::kPending) {
      return OnDeviceModelEligibilityReason::kValidationPending;
    } else if (state.result != OnDeviceModelValidationResult::kSuccess) {
      return OnDeviceModelEligibilityReason::kValidationFailed;
    }
  }
  return OnDeviceModelEligibilityReason::kSuccess;
}

void OnDeviceModelAccessController::OnResponseCompleted() {
  pref_service_->SetInteger(kOnDeviceModelCrashCount, 0);
  pref_service_->SetInteger(kOnDeviceModelTimeoutCount, 0);
}

void OnDeviceModelAccessController::OnDisconnectedFromRemote() {
  pref_service_->SetInteger(
      kOnDeviceModelCrashCount,
      pref_service_->GetInteger(kOnDeviceModelCrashCount) + 1);
}

void OnDeviceModelAccessController::OnGpuBlocked() {
  is_gpu_blocked_ = true;
}

void OnDeviceModelAccessController::OnSessionTimedOut() {
  pref_service_->SetInteger(
      kOnDeviceModelTimeoutCount,
      pref_service_->GetInteger(kOnDeviceModelTimeoutCount) + 1);
}

bool OnDeviceModelAccessController::ShouldValidateModel(
    std::string_view component_version) {
  if (!features::IsOnDeviceModelValidationEnabled()) {
    return false;
  }

  ValidationState state = GetValidationState();
  if (state.component_version != component_version) {
    state = ValidationState();
  }
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelValidationResultOnValidationStarted",
      state.result);
  // Don't need to re-validate on success.
  if (state.result == OnDeviceModelValidationResult::kSuccess) {
    return false;
  }

  state.attempt_count++;
  state.component_version = component_version;
  state.result = OnDeviceModelValidationResult::kPending;
  SetValidationState(state);
  if (state.attempt_count >
      features::GetOnDeviceModelValidationAttemptCount()) {
    return false;
  }
  return true;
}

void OnDeviceModelAccessController::OnValidationFinished(
    OnDeviceModelValidationResult result) {
  ValidationState state = GetValidationState();
  state.result = result;
  SetValidationState(state);
}

OnDeviceModelAccessController::ValidationState
OnDeviceModelAccessController::GetValidationState() const {
  const auto& dict = pref_service_->GetDict(kOnDeviceModelValidationResult);
  ValidationState state;
  if (const std::string* component_version =
          dict.FindString(kComponentVersionKey)) {
    state.component_version = *component_version;
  }
  state.result = ConvertToOnDeviceModelValidationResult(
      dict.FindInt(kResultKey)
          .value_or(static_cast<int>(OnDeviceModelValidationResult::kUnknown)));
  state.attempt_count = dict.FindInt(kAttemptCountKey).value_or(0);
  return state;
}

void OnDeviceModelAccessController::SetValidationState(
    const ValidationState& state) {
  base::Value::Dict dict;
  dict.Set(kResultKey, static_cast<int>(state.result));
  dict.Set(kAttemptCountKey, state.attempt_count);
  dict.Set(kComponentVersionKey, state.component_version);
  pref_service_->SetDict(kOnDeviceModelValidationResult, std::move(dict));
}

}  // namespace optimization_guide
