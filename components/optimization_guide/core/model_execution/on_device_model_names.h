// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_NAMES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_NAMES_H_

#include <string>

#include "base/files/file_path.h"

namespace optimization_guide {

// LINT.IfChange(OnDeviceBaseModelEnum)
// Enums for the base model that is installed on-device.
// These must remain in sync with InstalledModel in
// tools/metrics/histograms/metadata/optimization/enums.xml.
enum class OnDeviceBaseModel {
  kUnknown = 0,
  kXxs = 1,
  kXs = 2,
  kV2Nano = 3,
  kV3Nano = 4,
  kV3NanoCpu = 5,
  kV3NanoGpu = 6,
  kGemma4 = 7,
  kClassifier = 8,
  kLanguageDetection = 9,
  kProofreaderSmallExpert = 10,
  kSpeechRecognitionSmallExpert = 11,
  kSummarizerSmallExpert = 12,
  kMaxValue = kSummarizerSmallExpert,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:OnDeviceBaseModelEnum)

// Converts a model name string (from BaseModelSpec) to the OnDeviceBaseModel
// enum.
OnDeviceBaseModel ConvertModelNameToEnum(const std::string& model_name);

// Converts a model name (from BaseModelSpec) to the UMA BaseModel variant name.
std::string ConvertModelNameToUmaModelName(const std::string& model_name);

// Converts a component key string (from manifest) to the OnDeviceBaseModel
// enum.
OnDeviceBaseModel ConvertComponentKeyToEnum(const std::string& key);

// Converts a component key to the UMA BaseModel variant name.
// These must remain in sync with BaseModel variants in
// tools/metrics/histograms/metadata/optimization/histograms.xml.
std::string ConvertComponentKeyToUmaModelName(const std::string& key);

// Files expected to be in the on-device model bundle.
inline constexpr base::FilePath::CharType kWeightsFile[] =
    FILE_PATH_LITERAL("weights.bin");
inline constexpr base::FilePath::CharType kWeightCacheFile[] =
    FILE_PATH_LITERAL("cache.bin");
inline constexpr base::FilePath::CharType kEncoderCacheFile[] =
    FILE_PATH_LITERAL("encoder_cache.bin");
inline constexpr base::FilePath::CharType kAdapterCacheFile[] =
    FILE_PATH_LITERAL("adapter_cache.bin");
inline constexpr base::FilePath::CharType kProgramCacheFile[] =
    FILE_PATH_LITERAL("program_cache.bin");
inline constexpr base::FilePath::CharType kOnDeviceModelExecutionConfigFile[] =
    FILE_PATH_LITERAL("on_device_model_execution_config.pb");
inline constexpr base::FilePath::CharType
    kOnDeviceModelAdaptationWeightsFile[] =
        FILE_PATH_LITERAL("adaptation_weights.bin");

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_NAMES_H_
