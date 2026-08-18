// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_switches.h"

#include <optional>

#include "base/command_line.h"
#include "google_apis/google_api_keys.h"

namespace optimization_guide {
namespace switches {

// Overrides the Optimization Guide model execution URL.
const char kOptimizationGuideServiceModelExecutionURL[] =
    "optimization-guide-service-model-execution-url";

const char kOptimizationGuideServiceModelExecutionDefaultURL[] =
    "https://chromemodelexecution-pa.googleapis.com/v1:Execute";

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

// Adds header to indicate to return debug logging data from the model execution
// service via response header.
const char kModelExecutionEnableRemoteDebugLogging[] =
    "optimization-guide-model-execution-enable-remote-debug-logging";

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

std::string GetModelQualityServiceAPIKey() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kModelQualityServiceAPIKey)) {
    return command_line->GetSwitchValueASCII(
        switches::kModelQualityServiceAPIKey);
  }

  return google_apis::GetAPIKey();
}

bool IsDebugLogsEnabled() {
  static bool enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kDebugLoggingEnabled);
  return enabled;
}

bool ShouldValidateModel() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kModelValidate);
}

bool ShouldValidateModelExecution() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kModelExecutionValidate);
}

std::optional<base::FilePath> GetOnDeviceModelExecutionOverride() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kOnDeviceModelExecutionOverride)) {
    return std::nullopt;
  }
  return command_line->GetSwitchValuePath(kOnDeviceModelExecutionOverride);
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

GURL GetModelExecutionServiceURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kOptimizationGuideServiceModelExecutionURL)) {
    return GURL(command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceModelExecutionURL));
  }
  return GURL(kOptimizationGuideServiceModelExecutionDefaultURL);
}

}  // namespace switches
}  // namespace optimization_guide
