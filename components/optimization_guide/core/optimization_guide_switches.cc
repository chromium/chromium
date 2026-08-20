// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_switches.h"

#include <optional>

#include "base/command_line.h"
#include "google_apis/google_api_keys.h"

namespace optimization_guide {
namespace switches {

const char kDebugLoggingEnabled[] = "enable-optimization-guide-debug-logs";

// Overrides the on-device model file paths for on-device model execution.
const char kOnDeviceModelExecutionOverride[] =
    "optimization-guide-ondevice-model-execution-override";

// Overrides the on-device model adaptation file paths for on-device model
// execution.
const char kOnDeviceModelAdaptationsOverride[] =
    "optimization-guide-ondevice-model-adaptations-override";

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

std::optional<base::FilePath> GetOnDeviceModelExecutionOverride() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kOnDeviceModelExecutionOverride)) {
    return std::nullopt;
  }
  return command_line->GetSwitchValuePath(kOnDeviceModelExecutionOverride);
}

bool ShouldGetFreeDiskSpaceWithUserVisiblePriorityTask() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kGetFreeDiskSpaceWithUserVisiblePriorityTask);
}

}  // namespace switches
}  // namespace optimization_guide
