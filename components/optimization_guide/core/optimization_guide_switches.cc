// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_switches.h"

#include <optional>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "google_apis/google_api_keys.h"

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

// Overrides the Optimization Guide model execution URL.
const char kOptimizationGuideServiceModelExecutionURL[] =
    "optimization-guide-service-model-execution-url";

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

// Overrides the on-device model file paths for on-device model execution.
const char kOnDeviceModelExecutionOverride[] =
    "optimization-guide-ondevice-model-execution-override";

// Overrides the on-device model adaptation file paths for on-device model
// execution.
const char kOnDeviceModelAdaptationsOverride[] =
    "optimization-guide-ondevice-model-adaptations-override";

// Enables the on-device model to run validation at startup after a delay. A
// text file can be provided used as input for the validation job and an output
// file path can be provided to write the response to.
const char kOnDeviceValidationRequestOverride[] =
    "ondevice-validation-request-override";
const char kOnDeviceValidationWriteToFile[] =
    "ondevice-validation-write-to-file";

// Triggers validation of the model. Used for manual testing.
const char kModelValidate[] = "optimization-guide-model-validate";

// Triggers validation of the server-side AI model execution. Used for
// integration testing.
const char kModelExecutionValidate[] =
    "optimization-guide-model-execution-validate";

// Overrides the model quality service URL.
const char kModelQualityServiceURL[] = "model-quality-service-url";

// Overrides the ModelQuality Service API Key for remote requests to be made.
const char kModelQualityServiceAPIKey[] = "model-quality-service-api-key";

// Enables model quality logs regardless of other client-side settings, as long
// as the client is a dogfood client.
const char kEnableModelQualityDogfoodLogging[] =
    "enable-model-quality-dogfood-logging";

const char kGetFreeDiskSpaceWithUserVisiblePriorityTask[] =
    "optimization-guide-get-free-disk-space-with-user-visible-priority-task";

// Allows sending an language code to the backend.
const char kOptimizationGuideLanguageOverride[] =
    "optimization-guide-language-override";

// Enables overriding Google API key configuration check for permissions.
const char kGoogleApiKeyConfigurationCheckOverride[] =
    "optimization-guide-google-api-key-configuration-check-override";

std::string GetModelQualityServiceAPIKey() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kModelQualityServiceAPIKey)) {
    return command_line->GetSwitchValueASCII(
        switches::kModelQualityServiceAPIKey);
  }

  return google_apis::GetAPIKey();
}

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
std::optional<std::vector<std::string>>
ParseHintsFetchOverrideFromCommandLine() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kFetchHintsOverride))
    return std::nullopt;

  std::string override_hosts_value =
      cmd_line->GetSwitchValueASCII(kFetchHintsOverride);

  std::vector<std::string> hosts =
      base::SplitString(override_hosts_value, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  if (hosts.size() == 0)
    return std::nullopt;

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

bool ShouldValidateModelExecution() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kModelExecutionValidate);
}

std::optional<std::string> GetModelOverride() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kModelOverride))
    return std::nullopt;
  return command_line->GetSwitchValueASCII(kModelOverride);
}

std::optional<std::string> GetOnDeviceModelExecutionOverride() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kOnDeviceModelExecutionOverride)) {
    return std::nullopt;
  }
  return command_line->GetSwitchValueASCII(kOnDeviceModelExecutionOverride);
}

std::optional<std::string> GetOnDeviceModelAdaptationsOverride() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kOnDeviceModelAdaptationsOverride)) {
    return std::nullopt;
  }
  return command_line->GetSwitchValueASCII(kOnDeviceModelAdaptationsOverride);
}

std::optional<base::FilePath> GetOnDeviceValidationRequestOverride() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kOnDeviceValidationRequestOverride)) {
    return std::nullopt;
  }
  return command_line->GetSwitchValuePath(kOnDeviceValidationRequestOverride);
}

std::optional<base::FilePath> GetOnDeviceValidationWriteToFile() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kOnDeviceValidationWriteToFile)) {
    return std::nullopt;
  }
  return command_line->GetSwitchValuePath(kOnDeviceValidationWriteToFile);
}

bool ShouldGetFreeDiskSpaceWithUserVisiblePriorityTask() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kGetFreeDiskSpaceWithUserVisiblePriorityTask);
}

bool ShouldSkipGoogleApiKeyConfigurationCheck() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kGoogleApiKeyConfigurationCheckOverride);
}

}  // namespace switches
}  // namespace optimization_guide
