// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_switches.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
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

// Overrides scheduling and time delays for fetching prediction models and host
// model features. This causes a prediction model and host model features fetch
// immediately on start up.
const char kFetchModelsAndHostModelFeaturesOverrideTimer[] =
    "optimization-guide-fetch-models-and-features-override";

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

bool ShouldOverrideFetchModelsAndFeaturesTimer() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kFetchModelsAndHostModelFeaturesOverrideTimer);
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

absl::optional<
    std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
GetModelOverrideForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kModelOverride))
    return absl::nullopt;

  std::string model_override_switch_value =
      command_line->GetSwitchValueASCII(kModelOverride);
  std::vector<std::string> model_overrides =
      base::SplitString(model_override_switch_value, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const auto& model_override : model_overrides) {
    std::vector<std::string> override_parts = base::SplitString(
        model_override, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (override_parts.size() != 2 && override_parts.size() != 3) {
      // Input is malformed.
      DLOG(ERROR) << "Invalid string format provided to the Model Override";
      return absl::nullopt;
    }

    optimization_guide::proto::OptimizationTarget recv_optimization_target;
    if (!optimization_guide::proto::OptimizationTarget_Parse(
            override_parts[0], &recv_optimization_target)) {
      // Optimization target is invalid.
      DLOG(ERROR)
          << "Invalid optimization target provided to the Model Override";
      return absl::nullopt;
    }
    if (optimization_target != recv_optimization_target)
      continue;

    if (override_parts.size() == 2) {
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>
          file_path_and_metadata =
              std::make_pair(override_parts[1], absl::nullopt);
      return file_path_and_metadata;
    }
    std::string binary_pb;
    if (!base::Base64Decode(override_parts[2], &binary_pb)) {
      DLOG(ERROR) << "Invalid base64 encoding of the Model Override";
      return absl::nullopt;
    }
    optimization_guide::proto::Any model_metadata;
    if (!model_metadata.ParseFromString(binary_pb)) {
      DLOG(ERROR) << "Invalid model metadata provided to the Model Override";
      return absl::nullopt;
    }
    std::pair<std::string, absl::optional<optimization_guide::proto::Any>>
        file_path_and_metadata =
            std::make_pair(override_parts[1], model_metadata);
    return file_path_and_metadata;
  }
  return absl::nullopt;
}

}  // namespace switches
}  // namespace optimization_guide
