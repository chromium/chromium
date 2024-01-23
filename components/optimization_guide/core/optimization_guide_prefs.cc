// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_prefs.h"

#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_registry_simple.h"

namespace optimization_guide {
namespace prefs {

// A pref that stores the last time a hints fetch was attempted. This limits the
// frequency that hints are fetched and prevents a crash loop that continually
// fetches hints on startup.
const char kHintsFetcherLastFetchAttempt[] =
    "optimization_guide.hintsfetcher.last_fetch_attempt";

// A pref that stores the last time a prediction model and host model features
// fetch was attempted. This limits the frequency of fetching for updates and
// prevents a crash loop that continually fetches prediction models and host
// model features on startup.
const char kModelAndFeaturesLastFetchAttempt[] =
    "optimization_guide.predictionmodelfetcher.last_fetch_attempt";

// A pref that stores the last time a prediction model fetch was successful.
// This helps determine when to schedule the next fetch.
const char kModelLastFetchSuccess[] =
    "optimization_guide.predictionmodelfetcher.last_fetch_success";

// A dictionary pref that stores hosts that have had hints successfully fetched
// from the remote Optimization Guide Server. The entry for each host contains
// the time that the fetch that covered this host expires, i.e., any hints
// from the fetch would be considered stale.
const char kHintsFetcherHostsSuccessfullyFetched[] =
    "optimization_guide.hintsfetcher.hosts_successfully_fetched";

// A string pref that stores the version of the Optimization Hints component
// that is currently being processed. This pref is cleared once processing
// completes. It is used for detecting a potential crash loop on processing a
// version of hints.
const char kPendingHintsProcessingVersion[] =
    "optimization_guide.pending_hints_processing_version";

// A dictionary pref that stores optimization type was previously
// registered so that the first run of optimization types can be identified.
// The entry is the OptimizationType enum. The value of the key-value pair will
// not be used.
const char kPreviouslyRegisteredOptimizationTypes[] =
    "optimization_guide.previously_registered_optimization_types";

// A dictionary pref that stores the file paths that need to be deleted as keys.
// The value will not be used.
const char kStoreFilePathsToDelete[] =
    "optimization_guide.store_file_paths_to_delete";

// An integer pref that contains the user's setting state of the opt-in
// main toggle. Changing this toggle affects the state of the per-feature
// toggles.
const char kModelExecutionMainToggleSettingState[] =
    "optimization_guide.model_execution_main_toggle_setting_state";

// A dictionary pref that stores optimization types that had filter associated
// with this type. The entry is the OptimizationType enum. The value of the
// key-value pair will not be used.
const char kPreviousOptimizationTypesWithFilter[] =
    "optimization_guide.previous_optimization_types_with_filter";

// Pref that contains user opt-in state for different features.
std::string GetSettingEnabledPrefName(proto::ModelExecutionFeature feature) {
  switch (feature) {
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE:
      return "optimization_guide.compose_setting_state";
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION:
      return "optimization_guide.tab_organization_setting_state";
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH:
      return "optimization_guide.wallpaper_search_setting_state";
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST:
    case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
      NOTREACHED();
      return "Invalid";
  }
}

void RegisterSettingsEnabledPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kModelExecutionMainToggleSettingState,
      static_cast<int>(FeatureOptInState::kNotInitialized));

  for (int i = proto::ModelExecutionFeature_MIN;
       i <= proto::ModelExecutionFeature_MAX; ++i) {
    proto::ModelExecutionFeature feature =
        static_cast<proto::ModelExecutionFeature>(i);
    switch (feature) {
      case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST:
      case proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED:
        continue;
      default:
        registry->RegisterIntegerPref(
            GetSettingEnabledPrefName(feature),
            static_cast<int>(FeatureOptInState::kNotInitialized));
    }
  }
}

namespace localstate {

// A dictionary pref that stores the lightweight metadata of all the models in
// the store, keyed by the optimization target and ModelCacheKey.
const char kModelStoreMetadata[] = "optimization_guide.model_store_metadata";

// A dictionary pref that stores the mapping between client generated
// ModelCacheKey based on the user profile characteristics and the server
// returned ModelCacheKey that was used in the actual model selection logic.
const char kModelCacheKeyMapping[] =
    "optimization_guide.model_cache_key_mapping";

// Preference of the last version checked. Used to determine when the
// disconnect count is reset.
const char kOnDeviceModelChromeVersion[] =
    "optimization_guide.on_device.last_version";

// Preference where number of disconnects (crashes) of on device model is
// stored.
const char kOnDeviceModelCrashCount[] =
    "optimization_guide.on_device.model_crash_count";

// Preference where number of timeouts of on device model is stored.
const char kOnDeviceModelTimeoutCount[] =
    "optimization_guide.on_device.timeout_count";

// Stores the last computed `OnDeviceModelPerformanceClass` of the device.
const char kOnDevicePerformanceClass[] =
    "optimization_guide.on_device.performance_class";

// Stores the version of the last downloaded on-device model.
const char kOnDeviceBaseModelVersion[] =
    "optimization_guide.on_device.base_model_version";

// Stores the name of the last downloaded on-device model.
const char kOnDeviceBaseModelName[] =
    "optimization_guide.on_device.base_model_name";

// A dictionary pref that stores the file paths that need to be deleted as keys.
// The value will not be used.
const char kStoreFilePathsToDelete[] =
    "optimization_guide.store_file_paths_to_delete";

// A timestamp for the last time a feature was used which could have benefited
// from the on-device model. We will use this to help decide whether to acquire
// the on device model.
const char kLastTimeOnDeviceEligibleFeatureWasUsed[] =
    "optimization_guide.last_time_on_device_eligible_feature_used";

// A timestamp for the last time the on-device model was eligible for download.
const char kLastTimeEligibleForOnDeviceModelDownload[] =
    "optimization_guide.on_device.last_time_eligible_for_download";

// An integer pref that contains the user's client id.
const char kModelQualityLogggingClientId[] =
    "optimization_guide.model_quality_logging_client_id";

}  // namespace localstate

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(
      kHintsFetcherLastFetchAttempt,
      base::Time().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterInt64Pref(
      kModelAndFeaturesLastFetchAttempt,
      base::Time().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterInt64Pref(kModelLastFetchSuccess, 0,
                              PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(kHintsFetcherHostsSuccessfullyFetched,
                                   PrefRegistry::LOSSY_PREF);

  registry->RegisterStringPref(kPendingHintsProcessingVersion, "",
                               PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(kPreviouslyRegisteredOptimizationTypes,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(kStoreFilePathsToDelete,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(kPreviousOptimizationTypesWithFilter,
                                   PrefRegistry::LOSSY_PREF);

  RegisterSettingsEnabledPrefs(registry);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(localstate::kOnDeviceModelChromeVersion,
                               std::string());
  registry->RegisterDictionaryPref(localstate::kModelStoreMetadata);
  registry->RegisterDictionaryPref(localstate::kModelCacheKeyMapping);
  registry->RegisterIntegerPref(localstate::kOnDeviceModelCrashCount, 0);
  registry->RegisterIntegerPref(localstate::kOnDeviceModelTimeoutCount, 0);
  registry->RegisterIntegerPref(localstate::kOnDevicePerformanceClass, 0);
  registry->RegisterStringPref(localstate::kOnDeviceBaseModelVersion,
                               std::string());
  registry->RegisterStringPref(localstate::kOnDeviceBaseModelName,
                               std::string());
  registry->RegisterDictionaryPref(localstate::kStoreFilePathsToDelete);
  registry->RegisterTimePref(
      localstate::kLastTimeOnDeviceEligibleFeatureWasUsed, base::Time::Min());
  registry->RegisterTimePref(
      localstate::kLastTimeEligibleForOnDeviceModelDownload, base::Time::Min());
  registry->RegisterInt64Pref(localstate::kModelQualityLogggingClientId, 0,
                              PrefRegistry::LOSSY_PREF);
}

}  // namespace prefs
}  // namespace optimization_guide
