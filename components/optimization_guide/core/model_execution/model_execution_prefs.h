// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_PREFS_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-forward.h"
#include "components/prefs/prefs_export.h"

class PrefRegistrySimple;
class PrefService;

namespace optimization_guide::model_execution::prefs {

enum class GenAILocalFoundationalModelEnterprisePolicySettings {
  kAllowed = 0,
  kDisallowed = 1,

  // Insert new values before this line.
  kMaxValue = kDisallowed,
};

// TODO: crbug.com/514743962 - This is a remote model execution thing.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void RegisterProfilePrefs(PrefRegistrySimple* registry);

namespace localstate {

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelChromeVersion[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelCrashCount[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelValidationResult[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDevicePerformanceClass[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDevicePerformanceClassVersion[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceVramMb[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDevicePerformanceClassGPUId[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kLastUsageByFeature[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kLastTimeEligibleForOnDeviceModelDownload[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kGenAILocalFoundationalModelEnterprisePolicySettings[];

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsLocalFoundationalModelEnterprisePolicyAllowed();

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceAiUserSettingsEnabled[];

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceAiAutoRemoveUnusedModelsEnabled[];

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kEmbeddingApiModelDownloadEligible[];

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kManifestAssetLedger[];

}  // namespace localstate

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace optimization_guide::model_execution::prefs

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_PREFS_H_
