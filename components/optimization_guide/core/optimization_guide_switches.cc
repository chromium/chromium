// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_switches.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/string_split.h"
#include "components/optimization_guide/proto/hints.pb.h"

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
base::Optional<std::vector<std::string>>
ParseHintsFetchOverrideFromCommandLine() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kFetchHintsOverride))
    return base::nullopt;

  std::string override_hosts_value =
      cmd_line->GetSwitchValueASCII(kFetchHintsOverride);

  std::vector<std::string> hosts =
      base::SplitString(override_hosts_value, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  if (hosts.size() == 0)
    return base::nullopt;

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

}  // namespace switches
}  // namespace optimization_guide
