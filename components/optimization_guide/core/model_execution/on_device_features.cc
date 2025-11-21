// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_features.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"

namespace optimization_guide {

std::string GetVariantName(mojom::OnDeviceFeature feature) {
  switch (feature) {
    case mojom::OnDeviceFeature::kCompose:
      return "Compose";
    case mojom::OnDeviceFeature::kTest:
      return "Test";
    case mojom::OnDeviceFeature::kPromptApi:
      return "PromptApi";
    case mojom::OnDeviceFeature::kHistorySearch:
      return "HistorySearch";
    case mojom::OnDeviceFeature::kSummarize:
      return "Summarize";
    case mojom::OnDeviceFeature::kHistoryQueryIntent:
      return "HistoryQueryIntent";
    case mojom::OnDeviceFeature::kScamDetection:
      return "ScamDetection";
    case mojom::OnDeviceFeature::kPermissionsAi:
      return "PermissionsAi";
    case mojom::OnDeviceFeature::kProofreaderApi:
      return "ProofreaderApi";
    case mojom::OnDeviceFeature::kWritingAssistanceApi:
      return "WritingAssistanceApi";
    case mojom::OnDeviceFeature::kOnDeviceSpeechRecognition:
      return "OnDeviceSpeechRecognition";
  }
}

// To enable on-device execution for a feature, update this to return a
// non-null target.
proto::OptimizationTarget GetOptimizationTargetForFeature(
    mojom::OnDeviceFeature feature) {
  switch (feature) {
    case mojom::OnDeviceFeature::kCompose:
      return proto::OPTIMIZATION_TARGET_COMPOSE;
    case mojom::OnDeviceFeature::kTest:
      return proto::OPTIMIZATION_TARGET_MODEL_VALIDATION;
    case mojom::OnDeviceFeature::kPromptApi:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_PROMPT_API;
    case mojom::OnDeviceFeature::kSummarize:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_SUMMARIZE;
    case mojom::OnDeviceFeature::kHistorySearch:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_HISTORY_SEARCH;
    case mojom::OnDeviceFeature::kHistoryQueryIntent:
      return proto::
          OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_HISTORY_QUERY_INTENT;
    case mojom::OnDeviceFeature::kScamDetection:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_SCAM_DETECTION;
    case mojom::OnDeviceFeature::kPermissionsAi:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_PERMISSIONS_AI;
    case mojom::OnDeviceFeature::kWritingAssistanceApi:
      return proto::
          OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_WRITING_ASSISTANCE_API;
    case mojom::OnDeviceFeature::kProofreaderApi:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_PROOFREADER_API;
    case mojom::OnDeviceFeature::kOnDeviceSpeechRecognition:
      return proto::
          OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_ON_DEVICE_SPEECH_RECOGNITION;
  }
}

proto::ModelExecutionFeature ToModelExecutionFeatureProto(
    mojom::OnDeviceFeature feature) {
  switch (feature) {
    case mojom::OnDeviceFeature::kCompose:
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE;
    case mojom::OnDeviceFeature::kTest:
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST;
    case mojom::OnDeviceFeature::kPromptApi:
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_PROMPT_API;
    case mojom::OnDeviceFeature::kSummarize:
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SUMMARIZE;
    case mojom::OnDeviceFeature::kHistorySearch:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_HISTORY_SEARCH;
    case mojom::OnDeviceFeature::kHistoryQueryIntent:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_HISTORY_QUERY_INTENT;
    case mojom::OnDeviceFeature::kScamDetection:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_SCAM_DETECTION;
    case mojom::OnDeviceFeature::kPermissionsAi:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_PERMISSIONS_AI;
    case mojom::OnDeviceFeature::kProofreaderApi:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_PROOFREADER_API;
    case mojom::OnDeviceFeature::kWritingAssistanceApi:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_WRITING_ASSISTANCE_API;
    case mojom::OnDeviceFeature::kOnDeviceSpeechRecognition:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_ON_DEVICE_SPEECH_RECOGNITION;
  }
}

std::optional<mojom::OnDeviceFeature> ToOnDeviceFeature(
    proto::ModelExecutionFeature feature) {
  static base::NoDestructor<
      base::flat_map<proto::ModelExecutionFeature, mojom::OnDeviceFeature>>
      lookup{base::MakeFlatMap<proto::ModelExecutionFeature,
                               mojom::OnDeviceFeature>(
          OnDeviceFeatureSet::All(), {}, [](mojom::OnDeviceFeature feature) {
            return std::pair(ToModelExecutionFeatureProto(feature), feature);
          })};
  auto it = lookup->find(feature);
  if (it == lookup->end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace optimization_guide
