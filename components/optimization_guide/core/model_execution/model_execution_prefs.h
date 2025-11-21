// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_PREFS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_PREFS_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "components/prefs/prefs_export.h"

class PrefRegistrySimple;
class PrefService;

namespace optimization_guide::model_execution::prefs {

// The possible values for the model execution enterprise policy.
// LINT.IfChange(ModelExecutionEnterprisePolicyValue)
enum class ModelExecutionEnterprisePolicyValue {
  kAllow = 0,
  kAllowWithoutLogging = 1,
  kDisable = 2,
};
// LINT.ThenChange(/chrome/browser/resources/settings/ai_page/constants.ts:ModelExecutionEnterprisePolicyValue)

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
extern const char kOnDeviceModelValidationResult[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDevicePerformanceClass[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDevicePerformanceClassVersion[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kLastUsageByFeature[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kLastTimeEligibleForOnDeviceModelDownload[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelQualityLoggingClientId[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kGenAILocalFoundationalModelEnterprisePolicySettings[];

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsLocalFoundationalModelEnterprisePolicyAllowed();

}  // namespace localstate

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void RegisterLegacyUsagePrefsForMigration(PrefRegistrySimple* registry);

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void MigrateLegacyUsagePrefs(PrefService* local_state);

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void PruneOldUsagePrefs(PrefService* local_state);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
void RecordFeatureUsage(PrefService* local_state,
                        mojom::OnDeviceFeature feature);
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool WasFeatureRecentlyUsed(const PrefService* local_state,
                            mojom::OnDeviceFeature feature);

}  // namespace optimization_guide::model_execution::prefs

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_EXECUTION_PREFS_H_
