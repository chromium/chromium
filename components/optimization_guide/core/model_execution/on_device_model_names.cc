// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_names.h"

namespace optimization_guide {

// LINT.IfChange(OnDeviceBaseModelName)
std::string ConvertBaseModelToUmaModelName(OnDeviceBaseModel base_model) {
  switch (base_model) {
    case OnDeviceBaseModel::kV3Nano:
      return "V3Nano";
    case OnDeviceBaseModel::kV3NanoCpu:
      return "V3NanoCpu";
    case OnDeviceBaseModel::kV3NanoGpu:
      return "V3NanoGpu";
    case OnDeviceBaseModel::kGemma4:
      return "Gemma4";
    case OnDeviceBaseModel::kClassifier:
      return "Classifier";
    case OnDeviceBaseModel::kLanguageDetection:
      return "LanguageDetection";
    case OnDeviceBaseModel::kProofreaderSmallExpert:
      return "ProofreaderSmallExpert";
    case OnDeviceBaseModel::kSpeechRecognitionSmallExpert:
      return "SpeechRecognitionSmallExpert";
    case OnDeviceBaseModel::kSummarizerSmallExpert:
      return "SummarizerSmallExpert";
    case OnDeviceBaseModel::kUnknown:
    case OnDeviceBaseModel::kXxs:
    case OnDeviceBaseModel::kXs:
    case OnDeviceBaseModel::kV2Nano:
      return "Unknown";
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/histograms.xml:OnDeviceBaseModelName)

OnDeviceBaseModel ConvertComponentKeyToEnum(const std::string& key) {
  if (key == "nano_v3_gpu_component") {
    return OnDeviceBaseModel::kV3NanoGpu;
  } else if (key == "nano_v3_cpu_component") {
    return OnDeviceBaseModel::kV3NanoCpu;
  } else if (key == "gemma4_component") {
    return OnDeviceBaseModel::kGemma4;
  } else if (key == "classifier_component") {
    return OnDeviceBaseModel::kClassifier;
  } else if (key == "language_detection_model_component") {
    return OnDeviceBaseModel::kLanguageDetection;
  } else if (key == "proofreader_small_expert_model_component") {
    return OnDeviceBaseModel::kProofreaderSmallExpert;
  } else if (key == "speech_recognition_small_expert_model_component") {
    return OnDeviceBaseModel::kSpeechRecognitionSmallExpert;
  } else if (key == "summarizer_small_expert_model_component") {
    return OnDeviceBaseModel::kSummarizerSmallExpert;
  } else {
    return OnDeviceBaseModel::kUnknown;
  }
}

std::string ConvertComponentKeyToUmaModelName(const std::string& key) {
  return ConvertBaseModelToUmaModelName(ConvertComponentKeyToEnum(key));
}

}  // namespace optimization_guide
