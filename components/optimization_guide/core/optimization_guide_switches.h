// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_

#include <memory>
#include <string>
#include <vector>

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
extern const char kOptimizationGuideServiceAPIKey[];
extern const char kPurgeHintsStore[];
extern const char kPurgeModelAndFeaturesStore[];
extern const char kDisableFetchingHintsAtNavigationStartForTesting[];
extern const char kDisableCheckingUserPermissionsForTesting[];
extern const char kDisableModelDownloadVerificationForTesting[];
extern const char kModelOverride[];
extern const char kDebugLoggingEnabled[];
extern const char kModelValidate[];
extern const char kStopHistoryVisitBatchAnnotateForTesting[];
extern const char kPageContentAnnotationsLoggingEnabled[];

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

// Whether to prevent annotations from happening when in a batch. For testing
// purposes only.
bool StopHistoryVisitBatchAnnotateForTesting();

// Returns true if page content annotations input should be logged.
bool ShouldLogPageContentAnnotationsInput();

}  // namespace switches
}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_SWITCHES_H_
