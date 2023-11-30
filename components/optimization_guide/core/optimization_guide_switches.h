// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/page_content_annotation_type.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
extern const char kDebugLoggingEnabled[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelValidate[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPageContentAnnotationsLoggingEnabled[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPageContentAnnotationsValidationStartupDelaySeconds[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPageContentAnnotationsValidationBatchSizeOverride[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPageContentAnnotationsValidationPageEntities[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPageContentAnnotationsValidationContentVisibility[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPageContentAnnotationsValidationTextEmbedding[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kPageContentAnnotationsValidationWriteToFile[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelQualityServiceURL[];
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
extern const char kModelQualityServiceAPIKey[];

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
absl::optional<std::vector<std::string>>
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

// Returns the model override command line switch.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
absl::optional<std::string> GetModelOverride();

// Returns the on-device model execution override command line switch.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
absl::optional<std::string> GetOnDeviceModelExecutionOverride();

// Returns true if debug logs are enabled for the optimization guide.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool IsDebugLogsEnabled();

// Returns true if page content annotations input should be logged.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool ShouldLogPageContentAnnotationsInput();

// Returns the delay to use for page content annotations validation, if given
// and valid on the command line.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
absl::optional<base::TimeDelta> PageContentAnnotationsValidationStartupDelay();

// Returns the size of the batch to use for page content annotations validation,
// if given and valid on the command line.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
absl::optional<size_t> PageContentAnnotationsValidationBatchSize();

// Whether the result of page content annotations validation should be sent to
// the console. True when any one of the corresponding command line flags is
// enabled.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
bool LogPageContentAnnotationsValidationToConsole();

// Returns a set on inputs to run the validation on for the given |type|,
// using comma separated input from the command line.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
absl::optional<std::vector<std::string>>
PageContentAnnotationsValidationInputForType(AnnotationType type);

// Returns the file path to write page content annotation validation results to.
COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
absl::optional<base::FilePath> PageContentAnnotationsValidationWriteToFile();

}  // namespace switches
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
