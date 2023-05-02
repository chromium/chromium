// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_switches.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {
namespace switches {

// Overrides the Hints Protobuf that would come from the component updater. If
// the value of this switch is invalid, regular hint processing is used.
// The value of this switch should be a base64 encoding of a binary
// Configuration message, found in optimization_guide's hints.proto. Providing a
// valid value to this switch causes Chrome startup to block on hints parsing.
const char kHintsProtoOverride[] = "optimization_guide_hints_override";

// Overrides scheduling and time delays for fetching hints and causes a hints
// fetch immediately on start up using the provided comma separate lists of
// hosts.
const char kFetchHintsOverride[] = "optimization-guide-fetch-hints-override";

// Overrides the hints fetch scheduling and delay, causing a hints fetch
// immediately on start up using the TopHostProvider. This is meant for testing.
const char kFetchHintsOverrideTimer[] =
    "optimization-guide-fetch-hints-override-timer";

// Overrides the Optimization Guide Service URL that the HintsFetcher will
// request remote hints from.
const char kOptimizationGuideServiceGetHintsURL[] =
    "optimization-guide-service-get-hints-url";

// Overrides the Optimization Guide Service URL that the PredictionModelFetcher
// will request remote models and host features from.
const char kOptimizationGuideServiceGetModelsURL[] =
    "optimization-guide-service-get-models-url";

// Overrides the Optimization Guide Service API Key for remote requests to be
// made.
const char kOptimizationGuideServiceAPIKey[] =
    "optimization-guide-service-api-key";

// Purges the store containing fetched and component hints on startup, so that
// it's guaranteed to be using fresh data.
const char kPurgeHintsStore[] = "purge-optimization-guide-store";

// Purges the store containing prediction medels and host model features on
// startup, so that it's guaranteed to be using fresh data.
const char kPurgeModelAndFeaturesStore[] = "purge-model-and-features-store";

const char kDisableFetchingHintsAtNavigationStartForTesting[] =
    "disable-fetching-hints-at-navigation-start";

const char kDisableCheckingUserPermissionsForTesting[] =
    "disable-checking-optimization-guide-user-permissions";

const char kDisableModelDownloadVerificationForTesting[] =
    "disable-model-download-verification";

const char kDebugLoggingEnabled[] = "enable-optimization-guide-debug-logs";

// Disables the fetching of models and overrides the file path and metadata to
// be used for the session to use what's passed via command-line instead of what
// is already stored.
//
// We expect that the string be a comma-separated string of model overrides with
// each model override be: OPTIMIZATION_TARGET_STRING:file_path or
// OPTIMIZATION_TARGET_STRING:file_path:base64_encoded_any_proto_model_metadata.
//
// It is possible this only works on Desktop since file paths are less easily
// accessible on Android, but may work.
const char kModelOverride[] = "optimization-guide-model-override";

// Triggers validation of the model. Used for manual testing.
const char kModelValidate[] = "optimization-guide-model-validate";

const char kPageContentAnnotationsLoggingEnabled[] =
    "enable-page-content-annotations-logging";

const char kPageContentAnnotationsValidationStartupDelaySeconds[] =
    "page-content-annotations-validation-startup-delay-seconds";

const char kPageContentAnnotationsValidationBatchSizeOverride[] =
    "page-content-annotations-validation-batch-size";

// Enables the specific annotation type to run validation at startup after a
// delay. A comma separated list of inputs can be given as a value which will be
// used as input for the validation job.
const char kPageContentAnnotationsValidationPageEntities[] =
    "page-content-annotations-validation-page-entities";
const char kPageContentAnnotationsValidationContentVisibility[] =
    "page-content-annotations-validation-content-visibility";

// Writes the output of page content annotation validations to the given file.
const char kPageContentAnnotationsValidationWriteToFile[] =
    "page-content-annotations-validation-write-to-file";

bool IsHintComponentProcessingDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kHintsProtoOverride);
}

bool ShouldPurgeOptimizationGuideStoreOnStartup() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  return cmd_line->HasSwitch(kHintsProtoOverride) ||
         cmd_line->HasSwitch(kPurgeHintsStore);
}

bool ShouldPurgeModelAndFeaturesStoreOnStartup() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  return cmd_line->HasSwitch(kPurgeModelAndFeaturesStore);
}

bool IsDebugLogsEnabled() {
  static bool enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kDebugLoggingEnabled);
  return enabled;
}

// Parses a list of hosts to have hints fetched for. This overrides scheduling
// of the first hints fetch and forces it to occur immediately. If no hosts are
// provided, nullopt is returned.
absl::optional<std::vector<std::string>>
ParseHintsFetchOverrideFromCommandLine() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kFetchHintsOverride))
    return absl::nullopt;

  std::string override_hosts_value =
      cmd_line->GetSwitchValueASCII(kFetchHintsOverride);

  std::vector<std::string> hosts =
      base::SplitString(override_hosts_value, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  if (hosts.size() == 0)
    return absl::nullopt;

  return hosts;
}

bool ShouldOverrideFetchHintsTimer() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kFetchHintsOverrideTimer);
}

std::unique_ptr<optimization_guide::proto::Configuration>
ParseComponentConfigFromCommandLine() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kHintsProtoOverride))
    return nullptr;

  std::string b64_pb = cmd_line->GetSwitchValueASCII(kHintsProtoOverride);

  std::string binary_pb;
  if (!base::Base64Decode(b64_pb, &binary_pb)) {
    LOG(ERROR) << "Invalid base64 encoding of the Hints Proto Override";
    return nullptr;
  }

  std::unique_ptr<optimization_guide::proto::Configuration>
      proto_configuration =
          std::make_unique<optimization_guide::proto::Configuration>();
  if (!proto_configuration->ParseFromString(binary_pb)) {
    LOG(ERROR) << "Invalid proto provided to the Hints Proto Override";
    return nullptr;
  }

  return proto_configuration;
}

bool DisableFetchingHintsAtNavigationStartForTesting() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(
      kDisableFetchingHintsAtNavigationStartForTesting);
}

bool ShouldOverrideCheckingUserPermissionsToFetchHintsForTesting() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kDisableCheckingUserPermissionsForTesting);
}

bool ShouldSkipModelDownloadVerificationForTesting() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kDisableModelDownloadVerificationForTesting);
}

bool IsModelOverridePresent() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kModelOverride);
}

bool ShouldValidateModel() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kModelValidate);
}

absl::optional<std::string> GetModelOverride() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kModelOverride))
    return absl::nullopt;
  return command_line->GetSwitchValueASCII(kModelOverride);
}

bool ShouldLogPageContentAnnotationsInput() {
  static bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      kPageContentAnnotationsLoggingEnabled);
  return enabled;
}

absl::optional<base::TimeDelta> PageContentAnnotationsValidationStartupDelay() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          kPageContentAnnotationsValidationStartupDelaySeconds)) {
    return absl::nullopt;
  }

  std::string value = command_line->GetSwitchValueASCII(
      kPageContentAnnotationsValidationStartupDelaySeconds);

  size_t seconds = 0;
  if (base::StringToSizeT(value, &seconds)) {
    return base::Seconds(seconds);
  }
  return absl::nullopt;
}

absl::optional<size_t> PageContentAnnotationsValidationBatchSize() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          kPageContentAnnotationsValidationBatchSizeOverride)) {
    return absl::nullopt;
  }

  std::string value = command_line->GetSwitchValueASCII(
      kPageContentAnnotationsValidationBatchSizeOverride);

  size_t size = 0;
  if (base::StringToSizeT(value, &size)) {
    return size;
  }
  return absl::nullopt;
}

bool LogPageContentAnnotationsValidationToConsole() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(
             kPageContentAnnotationsValidationPageEntities) ||
         command_line->HasSwitch(
             kPageContentAnnotationsValidationContentVisibility);
}

absl::optional<std::vector<std::string>>
PageContentAnnotationsValidationInputForType(AnnotationType type) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string value;
  switch (type) {
    case AnnotationType::kPageEntities:
      value = command_line->GetSwitchValueASCII(
          kPageContentAnnotationsValidationPageEntities);
      break;
    case AnnotationType::kContentVisibility:
      value = command_line->GetSwitchValueASCII(
          kPageContentAnnotationsValidationContentVisibility);
      break;
    default:
      break;
  }
  if (value.empty()) {
    return absl::nullopt;
  }

  return base::SplitString(value, ",", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

absl::optional<base::FilePath> PageContentAnnotationsValidationWriteToFile() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kPageContentAnnotationsValidationWriteToFile)) {
    return absl::nullopt;
  }
  return command_line->GetSwitchValuePath(
      kPageContentAnnotationsValidationWriteToFile);
}

}  // namespace switches
}  // namespace optimization_guide
