// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_capability.h"

#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"

namespace optimization_guide {

std::ostream& operator<<(std::ostream& out,
                         const OnDeviceModelEligibilityReason& val) {
  switch (val) {
    case OnDeviceModelEligibilityReason::kUnknown:
      return out << "Unknown";
    case OnDeviceModelEligibilityReason::kSuccess:
      return out << "Success";
    case OnDeviceModelEligibilityReason::kFeatureNotEnabled:
      return out << "FeatureNotEnabled";
    case OnDeviceModelEligibilityReason::kDeprecatedModelNotAvailable:
      return out << "ModelNotAvailable";
    case OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature:
      return out << "ConfigNotAvailableForFeature";
    case OnDeviceModelEligibilityReason::kGpuBlocked:
      return out << "GpuBlocked";
    case OnDeviceModelEligibilityReason::kTooManyRecentCrashes:
      return out << "TooManyRecentCrashes";
    case OnDeviceModelEligibilityReason::kSafetyModelNotAvailable:
      return out << "SafetyModelNotAvailable";
    case OnDeviceModelEligibilityReason::kSafetyConfigNotAvailableForFeature:
      return out << "SafetyConfigNotAvailableForFeature";
    case OnDeviceModelEligibilityReason::kLanguageDetectionModelNotAvailable:
      return out << "LanguageDetectionModelNotAvailable";
    case OnDeviceModelEligibilityReason::kFeatureExecutionNotEnabled:
      return out << "FeatureExecutionNotEnabled";
    case OnDeviceModelEligibilityReason::kModelAdaptationNotAvailable:
      return out << "ModelAdaptationNotAvailable";
    case OnDeviceModelEligibilityReason::kValidationPending:
      return out << "ValidationPending";
    case OnDeviceModelEligibilityReason::kValidationFailed:
      return out << "ValidationFailed";
    case OnDeviceModelEligibilityReason::kModelToBeInstalled:
      return out << "ModelToBeInstalled";
    case OnDeviceModelEligibilityReason::kModelNotEligible:
      return out << "ModelNotEligible";
    case OnDeviceModelEligibilityReason::kInsufficientDiskSpace:
      return out << "InsufficientDiskSpace";
    case OnDeviceModelEligibilityReason::kNoOnDeviceFeatureUsed:
      return out << "NoOnDeviceFeatureUsed";
  }
  return out;
}

std::optional<mojom::ModelUnavailableReason> AvailabilityFromEligibilityReason(
    OnDeviceModelEligibilityReason reason) {
  switch (reason) {
    case OnDeviceModelEligibilityReason::kUnknown:
      return mojom::ModelUnavailableReason::kUnknown;
    case OnDeviceModelEligibilityReason::kSuccess:
      return std::nullopt;
    // Permanent errors.
    case OnDeviceModelEligibilityReason::kDeprecatedModelNotAvailable:
    case OnDeviceModelEligibilityReason::kFeatureNotEnabled:
    case OnDeviceModelEligibilityReason::kGpuBlocked:
    case OnDeviceModelEligibilityReason::kTooManyRecentCrashes:
    case OnDeviceModelEligibilityReason::kSafetyConfigNotAvailableForFeature:
    case OnDeviceModelEligibilityReason::kFeatureExecutionNotEnabled:
    case OnDeviceModelEligibilityReason::kValidationFailed:
    case OnDeviceModelEligibilityReason::kModelNotEligible:
    case OnDeviceModelEligibilityReason::kInsufficientDiskSpace:
    // This is returned if the device will never support a capability.
    case OnDeviceModelEligibilityReason::kModelAdaptationNotAvailable:
      return mojom::ModelUnavailableReason::kNotSupported;
    // Errors solved by request.
    case OnDeviceModelEligibilityReason::kNoOnDeviceFeatureUsed:
      return mojom::ModelUnavailableReason::kPendingUsage;
    // Errors solved by waiting.
    case OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature:
    case OnDeviceModelEligibilityReason::kSafetyModelNotAvailable:
    case OnDeviceModelEligibilityReason::kLanguageDetectionModelNotAvailable:
    case OnDeviceModelEligibilityReason::kValidationPending:
    case OnDeviceModelEligibilityReason::kModelToBeInstalled:
      return mojom::ModelUnavailableReason::kPendingAssets;
  }
}

OptimizationGuideModelStreamingExecutionResult::
    OptimizationGuideModelStreamingExecutionResult() = default;

OptimizationGuideModelStreamingExecutionResult::
    OptimizationGuideModelStreamingExecutionResult(
        base::expected<const StreamingResponse,
                       OptimizationGuideModelExecutionError> response,
        bool provided_by_on_device,
        std::unique_ptr<proto::ModelExecutionInfo> execution_info)
    : response(response),
      provided_by_on_device(provided_by_on_device),
      execution_info(std::move(execution_info)) {}

OptimizationGuideModelStreamingExecutionResult::
    ~OptimizationGuideModelStreamingExecutionResult() = default;

OptimizationGuideModelStreamingExecutionResult::
    OptimizationGuideModelStreamingExecutionResult(
        OptimizationGuideModelStreamingExecutionResult&& src) = default;

OnDeviceCapability::OnDeviceCapability() = default;
OnDeviceCapability::~OnDeviceCapability() = default;

std::unique_ptr<OnDeviceSession> OnDeviceCapability::StartSession(
    mojom::OnDeviceFeature feature,
    const SessionConfigParams& config_params,
    base::WeakPtr<OptimizationGuideLogger> logger) {
  return nullptr;
}

void OnDeviceCapability::AddOnDeviceModelAvailabilityChangeObserver(
    mojom::OnDeviceFeature feature,
    OnDeviceModelAvailabilityObserver* observer) {}

void OnDeviceCapability::RemoveOnDeviceModelAvailabilityChangeObserver(
    mojom::OnDeviceFeature feature,
    OnDeviceModelAvailabilityObserver* observer) {}

on_device_model::Capabilities OnDeviceCapability::GetOnDeviceCapabilities() {
  return {};
}

OnDeviceModelEligibilityReason OnDeviceCapability::GetOnDeviceModelEligibility(
    mojom::OnDeviceFeature feature) {
  return OnDeviceModelEligibilityReason::kFeatureNotEnabled;
}

void OnDeviceCapability::GetOnDeviceModelEligibilityAsync(
    mojom::OnDeviceFeature feature,
    const on_device_model::Capabilities& capabilities,
    base::OnceCallback<void(OnDeviceModelEligibilityReason)> callback) {
  std::move(callback).Run(OnDeviceModelEligibilityReason::kFeatureNotEnabled);
}

std::optional<SamplingParamsConfig> OnDeviceCapability::GetSamplingParamsConfig(
    mojom::OnDeviceFeature feature) {
  return std::nullopt;
}

std::optional<const optimization_guide::proto::Any>
OnDeviceCapability::GetFeatureMetadata(mojom::OnDeviceFeature feature) {
  return std::nullopt;
}
}  // namespace optimization_guide
