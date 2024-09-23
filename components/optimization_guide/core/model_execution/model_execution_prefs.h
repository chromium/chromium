// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_PREFS_H_

#include "base/component_export.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

class PrefRegistrySimple;
class PrefService;

namespace optimization_guide::model_execution::prefs {

// The possible values for the model execution enterprise policy.
enum class ModelExecutionEnterprisePolicyValue {
  kAllow = 0,
  kAllowWithoutLogging = 1,
  kDisable = 2,
};

enum class GenAILocalFoundationalModelEnterprisePolicySettings {
  kAllowed = 0,
  kDisallowed = 1,

  // Insert new values before this line.
  kMaxValue = kDisallowed,
};

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void RegisterProfilePrefs(PrefRegistrySimple* registry);

namespace localstate {

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelChromeVersion[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelCrashCount[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelTimeoutCount[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelValidationResult[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDevicePerformanceClass[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kLastTimeComposeWasUsed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kLastTimePromptApiWasUsed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kLastTimeSummarizeApiWasUsed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kLastTimeTestFeatureWasUsed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kLastTimeHistorySearchWasUsed[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kLastTimeEligibleForOnDeviceModelDownload[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelQualityLogggingClientId[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kGenAILocalFoundationalModelEnterprisePolicySettings[];

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsLocalFoundationalModelEnterprisePolicyAllowed();

}  // namespace localstate

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Returns the value of the local state pref to check for whether an on-device
// eligible `feature` was recently used. All on-device eligible features should
// have this pref defined.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
const char* GetOnDeviceFeatureRecentlyUsedPref(ModelBasedCapabilityKey feature);

}  // namespace optimization_guide::model_execution::prefs

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_PREFS_H_
