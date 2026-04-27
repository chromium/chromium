// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_features.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"

namespace optimization_guide {

std::string_view GetVariantName(mojom::OnDeviceFeature feature) {
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
    case mojom::OnDeviceFeature::kSpeechRecognitionSmallExpertModel:
      return "SpeechRecognitionSmallExpertModel";
    case mojom::OnDeviceFeature::kClassifier:
      return "Classifier";
  }
}

OnDeviceModelType GetOnDeviceModelType(mojom::OnDeviceFeature feature) {
  switch (feature) {
    case mojom::OnDeviceFeature::kClassifier:
      return OnDeviceModelType::kClassifierModel;
    case mojom::OnDeviceFeature::kCompose:
    case mojom::OnDeviceFeature::kTest:
    case mojom::OnDeviceFeature::kPromptApi:
    case mojom::OnDeviceFeature::kHistorySearch:
    case mojom::OnDeviceFeature::kSummarize:
    case mojom::OnDeviceFeature::kHistoryQueryIntent:
    case mojom::OnDeviceFeature::kScamDetection:
    case mojom::OnDeviceFeature::kPermissionsAi:
    case mojom::OnDeviceFeature::kProofreaderApi:
    case mojom::OnDeviceFeature::kWritingAssistanceApi:
    case mojom::OnDeviceFeature::kOnDeviceSpeechRecognition:
    case mojom::OnDeviceFeature::kSpeechRecognitionSmallExpertModel:
      return OnDeviceModelType::kBaseModel;
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
    case mojom::OnDeviceFeature::kSpeechRecognitionSmallExpertModel:
      return proto::
          OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_ON_DEVICE_SPEECH_RECOGNITION_TINY_GEMMA;
    case mojom::OnDeviceFeature::kClassifier:
      return proto::OPTIMIZATION_TARGET_MODEL_EXECUTION_FEATURE_CLASSIFIER;
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
    case mojom::OnDeviceFeature::kSpeechRecognitionSmallExpertModel:
      return proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_ON_DEVICE_SPEECH_RECOGNITION_TINY_GEMMA;
    case mojom::OnDeviceFeature::kClassifier:
      return proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_CLASSIFIER;
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

std::string ToUseCaseName(mojom::OnDeviceFeature feature) {
  switch (feature) {
    case mojom::OnDeviceFeature::kCompose:
      return "compose";
    case mojom::OnDeviceFeature::kTest:
      return "test";
    case mojom::OnDeviceFeature::kPromptApi:
      return "prompt_api";
    case mojom::OnDeviceFeature::kHistorySearch:
      return "history_search";
    case mojom::OnDeviceFeature::kSummarize:
      return "summarizer_api";
    case mojom::OnDeviceFeature::kHistoryQueryIntent:
      return "history_query_intent";
    case mojom::OnDeviceFeature::kScamDetection:
      return "scam_detection";
    case mojom::OnDeviceFeature::kPermissionsAi:
      return "permissions_ai";
    case mojom::OnDeviceFeature::kWritingAssistanceApi:
      return "writer_assistance_api";
    case mojom::OnDeviceFeature::kProofreaderApi:
      return "proofreader_api";
    case mojom::OnDeviceFeature::kOnDeviceSpeechRecognition:
      return "speech_recognition";
    case mojom::OnDeviceFeature::kSpeechRecognitionSmallExpertModel:
      return "speech_recognition_tinygemma";
    case mojom::OnDeviceFeature::kClassifier:
      return "classifier_api";
  }
}

std::optional<mojom::OnDeviceFeature> GetFeatureForUseCase(
    const std::string& use_case_name) {
  static base::NoDestructor<base::flat_map<std::string, mojom::OnDeviceFeature>>
      lookup{[]() {
        base::flat_map<std::string, mojom::OnDeviceFeature> map;
        for (auto feature : OnDeviceFeatureSet::All()) {
          map[ToUseCaseName(feature)] = feature;
        }
        return map;
      }()};

  auto it = lookup->find(use_case_name);
  if (it == lookup->end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace optimization_guide
