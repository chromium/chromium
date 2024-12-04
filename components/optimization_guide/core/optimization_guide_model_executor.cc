// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_model_executor.h"

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
    case OnDeviceModelEligibilityReason::kTooManyRecentTimeouts:
      return out << "TooManyRecentTimeouts";
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

OptimizationGuideModelExecutionResult::OptimizationGuideModelExecutionResult() =
    default;

OptimizationGuideModelExecutionResult::OptimizationGuideModelExecutionResult(
    OptimizationGuideModelExecutionResult&& other) = default;

OptimizationGuideModelExecutionResult::
    ~OptimizationGuideModelExecutionResult() = default;

OptimizationGuideModelExecutionResult::OptimizationGuideModelExecutionResult(
    base::expected<const proto::Any /*response_metadata*/,
                   OptimizationGuideModelExecutionError> response,
    std::unique_ptr<proto::ModelExecutionInfo> execution_info)
    : response(response), execution_info(std::move(execution_info)) {}

OptimizationGuideModelStreamingExecutionResult::
    OptimizationGuideModelStreamingExecutionResult() = default;

OptimizationGuideModelStreamingExecutionResult::
    OptimizationGuideModelStreamingExecutionResult(
        base::expected<const StreamingResponse,
                       OptimizationGuideModelExecutionError> response,
        bool provided_by_on_device,
        std::unique_ptr<ModelQualityLogEntry> log_entry,
        std::unique_ptr<proto::ModelExecutionInfo> execution_info)
    : response(response),
      provided_by_on_device(provided_by_on_device),
      log_entry(std::move(log_entry)),
      execution_info(std::move(execution_info)) {}

OptimizationGuideModelStreamingExecutionResult::
    ~OptimizationGuideModelStreamingExecutionResult() = default;

OptimizationGuideModelStreamingExecutionResult::
    OptimizationGuideModelStreamingExecutionResult(
        OptimizationGuideModelStreamingExecutionResult&& src) = default;

}  // namespace optimization_guide
