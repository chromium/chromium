// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

namespace optimization_guide {
namespace proto {
class Configuration;
}  // namespace proto

namespace switches {

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kHintsProtoOverride[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kFetchHintsOverride[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kFetchHintsOverrideTimer[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideServiceGetHintsURL[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideServiceGetModelsURL[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideServiceModelExecutionURL[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideServiceAPIKey[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPurgeHintsStore[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPurgeModelAndFeaturesStore[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kDisableFetchingHintsAtNavigationStartForTesting[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kDisableCheckingUserPermissionsForTesting[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kDisableModelDownloadVerificationForTesting[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelOverride[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelExecutionOverride[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceModelAdaptationsOverride[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceValidationRequestOverride[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOnDeviceValidationWriteToFile[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kDebugLoggingEnabled[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelValidate[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelExecutionValidate[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelQualityServiceURL[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelQualityServiceAPIKey[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kEnableModelQualityDogfoodLogging[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kGetFreeDiskSpaceWithUserVisiblePriorityTask[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kOptimizationGuideLanguageOverride[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kGoogleApiKeyConfigurationCheckOverride[];

// The API key for the ModelQualityLoggingService.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::string GetModelQualityServiceAPIKey();

// Returns whether the hint component should be processed.
// Available hint components are only processed if a proto override isn't being
// used; otherwise, the hints from the proto override are used instead.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsHintComponentProcessingDisabled();

// Returns whether all entries within the store should be purged during startup
// if the explicit purge switch exists or if a proto override is being used, in
// which case the hints need to come from the override instead.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldPurgeOptimizationGuideStoreOnStartup();

// Returns whether all entries within the store should be purged during startup
// if the explicit purge switch exists.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldPurgeModelAndFeaturesStoreOnStartup();

// Parses a list of hosts to have hints fetched for. This overrides scheduling
// of the first hints fetch and forces it to occur immediately. If no hosts are
// provided, nullopt is returned.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<std::vector<std::string>>
ParseHintsFetchOverrideFromCommandLine();

// Whether the hints fetcher timer should be overridden.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldOverrideFetchHintsTimer();

// Attempts to parse a base64 encoded Optimization Guide Configuration proto
// from the command line. If no proto is given or if it is encoded incorrectly,
// nullptr is returned.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::unique_ptr<optimization_guide::proto::Configuration>
ParseComponentConfigFromCommandLine();

// Returns true if fetching of hints in real-time at the time of navigation
// start should be disabled. Returns true only in tests.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool DisableFetchingHintsAtNavigationStartForTesting();

// Returns true if checking of the user's permissions to fetch hints from the
// remote Optimization Guide Service should be ignored. Returns true only in
// tests.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldOverrideCheckingUserPermissionsToFetchHintsForTesting();

// Returns true if the verification of model downloads should be skipped.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldSkipModelDownloadVerificationForTesting();

// Returns whether at least one model was provided via command-line.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsModelOverridePresent();

// Returns whether the model validation should happen.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldValidateModel();

// Returns whether the server-side AI model execution validation should happen.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldValidateModelExecution();

// Returns the model override command line switch.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<std::string> GetModelOverride();

// Returns the on-device model execution override command line switch.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<std::string> GetOnDeviceModelExecutionOverride();

// Returns the on-device model adaptations override command line switch.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<std::string> GetOnDeviceModelAdaptationsOverride();

// Returns the file path to the text file to use for the on-device request
// override.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<base::FilePath> GetOnDeviceValidationRequestOverride();

// Returns the file path to write the on-device validation response to.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::optional<base::FilePath> GetOnDeviceValidationWriteToFile();

// Returns true if debug logs are enabled for the optimization guide.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsDebugLogsEnabled();

// Returns whether to get free disk space with base::TaskPriority::USER_VISIBLE
// task. This is about the freediskspace check in the context of the on-device
// model eligibility check.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldGetFreeDiskSpaceWithUserVisiblePriorityTask();

// Returns true if Google API key configuration check should be skipped.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldSkipGoogleApiKeyConfigurationCheck();

}  // namespace switches
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
