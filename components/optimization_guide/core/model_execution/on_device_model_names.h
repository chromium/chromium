// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_NAMES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_NAMES_H_

#include <optional>
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
  kLayaEnWint8 = 13,
  kLayaMlWint8 = 14,
  kMaxValue = kLayaMlWint8,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/enums.xml:OnDeviceBaseModelEnum)

// Converts an OnDeviceBaseModel enum to the UMA BaseModel variant name.
// These must remain in sync with BaseModel variants in
// tools/metrics/histograms/metadata/optimization/histograms.xml.
// TODO(crbug.com/582309438): Move model metadata (including logging/UMA enums
// and names) into the manifest as explicit data.
std::string ConvertBaseModelToUmaModelName(OnDeviceBaseModel base_model);

// Converts a component key string (from manifest) to the OnDeviceBaseModel
// enum.
// TODO(crbug.com/582309438): Move model metadata into the manifest
// as explicit data.
OnDeviceBaseModel ConvertComponentKeyToEnum(const std::string& key);

// Converts a component key to the UMA BaseModel variant name.
// These must remain in sync with BaseModel variants in
// tools/metrics/histograms/metadata/optimization/histograms.xml.
// TODO(crbug.com/582309438): Move model metadata (including
// logging/UMA enums and names) into the manifest as explicit data.
std::string ConvertComponentKeyToUmaModelName(const std::string& key);

// User-visible grouping of base models. Models that are not mapped to a
// category return std::nullopt and are hidden from user-facing surfaces until
// approved.
enum class OnDeviceModelCategory {
  kGeneralPurpose = 0,
  kProofreading = 1,
  kSummarizing = 2,
  kLiveTranscription = 3,
  kMaxValue = kLiveTranscription,
};

// Returns the user-visible category for `base_model`, or std::nullopt if the
// model should be hidden from user-facing surfaces (e.g. unapproved Dev Trial
// models, shared safety/language-detection models).
// TODO(crbug.com/582309438): Move model category metadata into the
// manifest as explicit data.
std::optional<OnDeviceModelCategory> GetOnDeviceModelCategory(
    OnDeviceBaseModel base_model);

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
