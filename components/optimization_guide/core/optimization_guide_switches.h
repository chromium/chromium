// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: crbug.com/514743962 - All of these switches should be moved to more
// specific files and out of this file.  Do not add anything here.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "url/gurl.h"

namespace optimization_guide {
namespace proto {
class Configuration;
}  // namespace proto

namespace switches {

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/hints/hints_manager.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kHintsProtoOverride[];
// Returns whether the hint component should be processed.
// Available hint components are only processed if a proto override isn't being
// used; otherwise, the hints from the proto override are used instead.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsHintComponentProcessingDisabled();
// Attempts to parse a base64 encoded Optimization Guide Configuration proto
// from the command line. If no proto is given or if it is encoded incorrectly,
// nullptr is returned.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::unique_ptr<optimization_guide::proto::Configuration>
ParseComponentConfigFromCommandLine();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kFetchHintsOverrideTimer[];
// Whether the hints fetcher timer should be overridden.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldOverrideFetchHintsTimer();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPurgeHintsStore[];
// Returns whether all entries within the store should be purged during startup
// if the explicit purge switch exists or if a proto override is being used, in
// which case the hints need to come from the override instead.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldPurgeOptimizationGuideStoreOnStartup();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kDisableFetchingHintsAtNavigationStartForTesting[];
// Returns true if fetching of hints in real-time at the time of navigation
// start should be disabled. Returns true only in tests.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool DisableFetchingHintsAtNavigationStartForTesting();

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/hints/command_line_top_host_provider.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kFetchHintsOverride[];
// Parses a list of hosts to have hints fetched for. This overrides scheduling
// of the first hints fetch and forces it to occur immediately. If no hosts are
// provided, nullopt is returned.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<std::vector<std::string>>
ParseHintsFetchOverrideFromCommandLine();

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/hints/hints_fetcher.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideServiceGetHintsURL[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideLanguageOverride[];

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/optimization_guide_permissions_util.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kDisableCheckingUserPermissionsForTesting[];
// Returns true if checking of the user's permissions to fetch hints from the
// remote Optimization Guide Service should be ignored. Returns true only in
// tests.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldOverrideCheckingUserPermissionsToFetchHintsForTesting();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kGoogleApiKeyConfigurationCheckOverride[];
// Returns true if Google API key configuration check should be skipped.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldSkipGoogleApiKeyConfigurationCheck();

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/optimization_guide_features.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideServiceGetModelsURL[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideServiceAPIKey[];

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/delivery/model_store_metadata_entry.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPurgeModelAndFeaturesStore[];
// Returns whether all entries within the store should be purged during startup
// if the explicit purge switch exists.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldPurgeModelAndFeaturesStoreOnStartup();

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/delivery/prediction_model_download_manager.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kDisableModelDownloadVerificationForTesting[];
// Returns true if the verification of model downloads should be skipped.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldSkipModelDownloadVerificationForTesting();

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/delivery/prediction_model_override.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelOverride[];

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/inference/model_validator.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelValidate[];
// Returns whether the model validation should happen.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldValidateModel();

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/model_execution/model_execution_manager.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideServiceModelExecutionURL[];
// Return the URL endpoint used for the model execution service.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
GURL GetModelExecutionServiceURL();

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/model_execution/model_execution_fetcher_impl.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelExecutionEnableRemoteDebugLogging[];

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/model_execution/on_device_model_component.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelExecutionOverride[];
// Returns the path to the on-device base model provided on the command line.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<base::FilePath> GetOnDeviceModelExecutionOverride();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kGetFreeDiskSpaceWithUserVisiblePriorityTask[];
// Returns whether to get free disk space with base::TaskPriority::USER_VISIBLE
// task. This is about the freediskspace check in the context of the on-device
// model eligibility check.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldGetFreeDiskSpaceWithUserVisiblePriorityTask();

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelAdaptationsOverride[];

// TODO(crbug.com/514743962): Move to chrome/browser/optimization_guide/model_validator_keyed_service.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelExecutionValidate[];
// Returns whether the server-side AI model execution validation should happen.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldValidateModelExecution();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceValidationRequestOverride[];
// Returns the file path to the text file to use for the on-device request
// override.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<base::FilePath> GetOnDeviceValidationRequestOverride();
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceValidationWriteToFile[];
// Returns the file path to write the on-device validation response to.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<base::FilePath> GetOnDeviceValidationWriteToFile();

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelQualityServiceURL[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelQualityServiceAPIKey[];
// The API key for the ModelQualityLoggingService.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::string GetModelQualityServiceAPIKey();

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/model_execution/model_execution_features_controller.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kEnableModelQualityDogfoodLogging[];

// TODO(crbug.com/514743962): Move to components/optimization_guide/core/optimization_guide_logger.h.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kDebugLoggingEnabled[];
// Returns true if debug logs are enabled for the optimization guide.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsDebugLogsEnabled();

}  // namespace switches
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
