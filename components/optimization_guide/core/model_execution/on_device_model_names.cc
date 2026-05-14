// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_names.h"

namespace optimization_guide {

OnDeviceBaseModel ConvertModelNameToEnum(const std::string& model_name) {
  if (model_name == "v3Nano") {
    return OnDeviceBaseModel::kV3Nano;
  } else if (model_name == "v2Nano") {
    return OnDeviceBaseModel::kV2Nano;
  } else if (model_name == "XS") {
    return OnDeviceBaseModel::kXs;
  } else if (model_name == "XXS") {
    return OnDeviceBaseModel::kXxs;
  } else {
    return OnDeviceBaseModel::kUnknown;
  }
}

std::string ConvertModelNameToUmaModelName(const std::string& model_name) {
  if (model_name == "v3Nano") {
    return "V3Nano";
  } else {
    return "Unknown";
  }
}

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

// LINT.IfChange(OnDeviceBaseModelName)
std::string ConvertComponentKeyToUmaModelName(const std::string& key) {
  if (key == "nano_v3_gpu_component") {
    return "V3NanoGpu";
  } else if (key == "nano_v3_cpu_component") {
    return "V3NanoCpu";
  } else if (key == "gemma4_component") {
    return "Gemma4";
  } else if (key == "classifier_component") {
    return "Classifier";
  } else if (key == "language_detection_model_component") {
    return "LanguageDetection";
  } else if (key == "proofreader_small_expert_model_component") {
    return "ProofreaderSmallExpert";
  } else if (key == "speech_recognition_small_expert_model_component") {
    return "SpeechRecognitionSmallExpert";
  } else if (key == "summarizer_small_expert_model_component") {
    return "SummarizerSmallExpert";
  } else {
    return "Unknown";
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/histograms.xml:OnDeviceBaseModelName)

}  // namespace optimization_guide
