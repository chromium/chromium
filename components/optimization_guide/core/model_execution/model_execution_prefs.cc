// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"

#include "components/optimization_guide/core/feature_registry/enterprise_policy_registry.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide::model_execution::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  RegisterGenAiFeatures(registry);
}

namespace localstate {

// Preference of the last version checked. Used to determine when the
// disconnect count is reset.
const char kOnDeviceModelChromeVersion[] =
    "optimization_guide.on_device.last_version";

// Preference where number of disconnects (crashes) of on device model is
// stored.
const char kOnDeviceModelCrashCount[] =
    "optimization_guide.on_device.model_crash_count";

const char kOnDeviceModelValidationResult[] =
    "optimization_guide.on_device.model_validation_result";

// Stores the last computed `OnDeviceModelPerformanceClass` of the device.
const char kOnDevicePerformanceClass[] =
    "optimization_guide.on_device.performance_class";

// Stores the last chrome version that the performance class was checked.
const char kOnDevicePerformanceClassVersion[] =
    "optimization_guide.on_device.performance_class_version";

// Stores the device VRAM in MB.
const char kOnDeviceVramMb[] = "optimization_guide.on_device.vram_mb";

// Stores the id of the GPU performance class was last checked on.
const char kOnDevicePerformanceClassGPUId[] =
    "optimization_guide.on_device.performance_class_gpu_id";

// Timestamps for the last time each features was used while on-device eligible.
// Used to decide which models are worth fetching.
const char kLastUsageByFeature[] =
    "optimization_guide.model_execution.last_usage_by_feature";

// A timestamp for the last time the on-device model was eligible for download.
const char kLastTimeEligibleForOnDeviceModelDownload[] =
    "optimization_guide.on_device.last_time_eligible_for_download";

// An integer pref that contains the user's client id.
const char kModelQualityLoggingClientId[] =
    "optimization_guide.model_quality_logging_client_id";

// An integer pref for the on-device GenAI foundational model enterprise policy
// settings.
const char kGenAILocalFoundationalModelEnterprisePolicySettings[] =
    "optimization_guide.gen_ai_local_foundational_model_settings";

// A boolean pref for the on-device GenAI foundational model user settings.
const char kOnDeviceAiUserSettingsEnabled[] =
    "optimization_guide.on_device_foundational_model_user_settings";

// Boolean pref indicating whether the AI embeddings model is eligible for
// download.
const char kEmbeddingApiModelDownloadEligible[] =
    "optimization_guide.on_device.embedding_api_model_download_eligible";

// A dictionary pref that tracks the state of assets managed by the manifest.
const char kManifestAssetLedger[] =
    "optimization_guide.model_execution.manifest_asset_ledger";

}  // namespace localstate

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(localstate::kOnDeviceModelChromeVersion,
                               std::string());
  registry->RegisterIntegerPref(localstate::kOnDeviceModelCrashCount, 0);
  registry->RegisterIntegerPref(localstate::kOnDevicePerformanceClass, 0);
  registry->RegisterStringPref(localstate::kOnDevicePerformanceClassVersion,
                               std::string());
  registry->RegisterUint64Pref(localstate::kOnDeviceVramMb, 0);
  registry->RegisterStringPref(localstate::kOnDevicePerformanceClassGPUId,
                               std::string());
  registry->RegisterTimePref(
      localstate::kLastTimeEligibleForOnDeviceModelDownload, base::Time::Min());
  registry->RegisterDictionaryPref(localstate::kOnDeviceModelValidationResult);
  registry->RegisterDictionaryPref(localstate::kLastUsageByFeature);
  registry->RegisterInt64Pref(localstate::kModelQualityLoggingClientId, 0,
                              PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(
      localstate::kGenAILocalFoundationalModelEnterprisePolicySettings, 0);
  registry->RegisterBooleanPref(localstate::kOnDeviceAiUserSettingsEnabled,
                                true);
  registry->RegisterBooleanPref(localstate::kEmbeddingApiModelDownloadEligible,
                                false);
  registry->RegisterDictionaryPref(localstate::kManifestAssetLedger);
}

}  // namespace optimization_guide::model_execution::prefs
