// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_

#include <memory>
#include <string>
#include <vector>

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

extern const char kHintsProtoOverride[];
extern const char kFetchHintsOverride[];
extern const char kFetchHintsOverrideTimer[];
extern const char kOptimizationGuideServiceGetHintsURL[];
extern const char kOptimizationGuideServiceGetModelsURL[];
extern const char kOptimizationGuideServiceModelExecutionURL[];
extern const char kOptimizationGuideServiceAPIKey[];
extern const char kPurgeHintsStore[];
extern const char kPurgeModelAndFeaturesStore[];
extern const char kDisableFetchingHintsAtNavigationStartForTesting[];
extern const char kDisableCheckingUserPermissionsForTesting[];
extern const char kDisableModelDownloadVerificationForTesting[];
extern const char kModelOverride[];
extern const char kDebugLoggingEnabled[];
extern const char kModelValidate[];
extern const char kPageContentAnnotationsLoggingEnabled[];
extern const char kPageContentAnnotationsValidationStartupDelaySeconds[];
extern const char kPageContentAnnotationsValidationBatchSizeOverride[];
extern const char kPageContentAnnotationsValidationPageEntities[];
extern const char kPageContentAnnotationsValidationContentVisibility[];
extern const char kPageContentAnnotationsValidationTextEmbedding[];
extern const char kPageContentAnnotationsValidationWriteToFile[];

// Returns whether the hint component should be processed.
// Available hint components are only processed if a proto override isn't being
// used; otherwise, the hints from the proto override are used instead.
bool IsHintComponentProcessingDisabled();

// Returns whether all entries within the store should be purged during startup
// if the explicit purge switch exists or if a proto override is being used, in
// which case the hints need to come from the override instead.
bool ShouldPurgeOptimizationGuideStoreOnStartup();

// Returns whether all entries within the store should be purged during startup
// if the explicit purge switch exists.
bool ShouldPurgeModelAndFeaturesStoreOnStartup();

// Parses a list of hosts to have hints fetched for. This overrides scheduling
// of the first hints fetch and forces it to occur immediately. If no hosts are
// provided, nullopt is returned.
absl::optional<std::vector<std::string>>
ParseHintsFetchOverrideFromCommandLine();

// Whether the hints fetcher timer should be overridden.
bool ShouldOverrideFetchHintsTimer();

// Attempts to parse a base64 encoded Optimization Guide Configuration proto
// from the command line. If no proto is given or if it is encoded incorrectly,
// nullptr is returned.
std::unique_ptr<optimization_guide::proto::Configuration>
ParseComponentConfigFromCommandLine();

// Returns true if fetching of hints in real-time at the time of navigation
// start should be disabled. Returns true only in tests.
bool DisableFetchingHintsAtNavigationStartForTesting();

// Returns true if checking of the user's permissions to fetch hints from the
// remote Optimization Guide Service should be ignored. Returns true only in
// tests.
bool ShouldOverrideCheckingUserPermissionsToFetchHintsForTesting();

// Returns true if the verification of model downloads should be skipped.
bool ShouldSkipModelDownloadVerificationForTesting();

// Returns whether at least one model was provided via command-line.
bool IsModelOverridePresent();

// Returns whether the model validation should happen.
bool ShouldValidateModel();

// Returns the model override command line switch.
absl::optional<std::string> GetModelOverride();

// Returns true if debug logs are enabled for the optimization guide.
bool IsDebugLogsEnabled();

// Returns true if page content annotations input should be logged.
bool ShouldLogPageContentAnnotationsInput();

// Returns the delay to use for page content annotations validation, if given
// and valid on the command line.
absl::optional<base::TimeDelta> PageContentAnnotationsValidationStartupDelay();

// Returns the size of the batch to use for page content annotations validation,
// if given and valid on the command line.
absl::optional<size_t> PageContentAnnotationsValidationBatchSize();

// Whether the result of page content annotations validation should be sent to
// the console. True when any one of the corresponding command line flags is
// enabled.
bool LogPageContentAnnotationsValidationToConsole();

// Returns a set on inputs to run the validation on for the given |type|,
// using comma separated input from the command line.
absl::optional<std::vector<std::string>>
PageContentAnnotationsValidationInputForType(AnnotationType type);

// Returns the file path to write page content annotation validation results to.
absl::optional<base::FilePath> PageContentAnnotationsValidationWriteToFile();

}  // namespace switches
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
