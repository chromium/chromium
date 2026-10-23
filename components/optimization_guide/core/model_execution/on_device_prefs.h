// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace optimization_guide::model_execution::prefs {

enum class GenAILocalFoundationalModelEnterprisePolicySettings {
  kAllowed = 0,
  kDisallowed = 1,

  // Insert new values before this line.
  kMaxValue = kDisallowed,
};

namespace localstate {

extern const char kOnDeviceModelChromeVersion[];
extern const char kOnDeviceModelCrashCount[];
extern const char kOnDeviceModelValidationResult[];
extern const char kOnDevicePerformanceClass[];
extern const char kOnDevicePerformanceClassVersion[];
extern const char kOnDeviceVramMb[];
extern const char kOnDevicePerformanceClassGPUId[];
extern const char kLastUsageByFeature[];
extern const char kLastTimeEligibleForOnDeviceModelDownload[];
extern const char kGenAILocalFoundationalModelEnterprisePolicySettings[];
extern const char kOnDeviceAiUserSettingsEnabled[];
extern const char kOnDeviceAiAutoRemoveUnusedModelsEnabled[];
extern const char kEmbeddingApiModelDownloadEligible[];
extern const char kManifestAssetLedger[];

}  // namespace localstate

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Returns the value of the GenAILocalFoundationalModelEnterprisePolicySettings
// pref from `local_state`.
GenAILocalFoundationalModelEnterprisePolicySettings
GetGenAILocalFoundationalModelEnterprisePolicySettings(
    const PrefService* local_state);

}  // namespace optimization_guide::model_execution::prefs

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_PREFS_H_
