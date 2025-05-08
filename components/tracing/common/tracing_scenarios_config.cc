// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/tracing_scenarios_config.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "components/tracing/common/tracing_switches.h"
#include "third_party/snappy/src/snappy.h"

namespace tracing {

BASE_FEATURE(kTracingTriggers,
             "TracingTriggers",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFieldTracing, "FieldTracing", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPresetTracing,
             "PresetTracing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kFieldTracingAnonymized,
                   &kFieldTracing,
                   "anonymized",
                   true);
BASE_FEATURE_PARAM(bool,
                   kFieldTracingForceUploads,
                   &kFieldTracing,
                   "force_uploads",
                   false);
BASE_FEATURE_PARAM(size_t,
                   kFieldTracingUploadLimitKb,
                   &kFieldTracing,
                   "upload_limit_kb",
                   0);
BASE_FEATURE_PARAM(bool,
                   kStartupFieldTracing,
                   &kFieldTracing,
                   "startup",
                   false);

namespace {

const base::FeatureParam<std::string> kTracingTriggerRulesConfig{
    &kTracingTriggers, "config", ""};
const base::FeatureParam<std::string> kFieldTracingConfig{&kFieldTracing,
                                                          "config", ""};
const base::FeatureParam<std::string> kPresetTracingConfig{&kPresetTracing,
                                                           "config", ""};

}  // namespace

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
GetPresetTracingScenariosConfig() {
  if (!base::FeatureList::IsEnabled(kPresetTracing)) {
    return std::nullopt;
  }
  return ParseEncodedTracingScenariosConfig(kPresetTracingConfig.Get());
}

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
GetFieldTracingScenariosConfig() {
  if (!base::FeatureList::IsEnabled(kFieldTracing)) {
    return std::nullopt;
  }
  return ParseEncodedTracingScenariosConfig(kFieldTracingConfig.Get());
}

std::optional<perfetto::protos::gen::TracingTriggerRulesConfig>
GetTracingTriggerRulesConfig() {
  if (!base::FeatureList::IsEnabled(kTracingTriggers)) {
    return std::nullopt;
  }
  std::string serialized_config;
  if (!base::Base64Decode(kTracingTriggerRulesConfig.Get(),
                          &serialized_config)) {
    return std::nullopt;
  }

  // `serialized_config` may optionally be compressed.
  std::string decompressed_config;
  if (snappy::Uncompress(serialized_config.data(), serialized_config.size(),
                         &decompressed_config)) {
    serialized_config = std::move(decompressed_config);
  }
  perfetto::protos::gen::TracingTriggerRulesConfig config;
  if (config.ParseFromString(serialized_config)) {
    return config;
  }
  return std::nullopt;
}

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
ParseSerializedTracingScenariosConfig(
    const base::span<const uint8_t>& config_bytes) {
  perfetto::protos::gen::ChromeFieldTracingConfig config;
  if (config_bytes.empty()) {
    return std::nullopt;
  }
  if (config.ParseFromArray(config_bytes.data(), config_bytes.size())) {
    return config;
  }
  return std::nullopt;
}

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
ParseEncodedTracingScenariosConfig(const std::string& config_string) {
  std::string serialized_config;
  if (!base::Base64Decode(config_string, &serialized_config,
                          base::Base64DecodePolicy::kForgiving)) {
    return std::nullopt;
  }

  // `serialized_config` may optionally be compressed.
  std::string decompressed_config;
  if (!snappy::Uncompress(serialized_config.data(), serialized_config.size(),
                          &decompressed_config)) {
    return ParseSerializedTracingScenariosConfig(
        base::as_byte_span(serialized_config));
  }

  return ParseSerializedTracingScenariosConfig(
      base::as_byte_span(decompressed_config));
}

bool IsBackgroundTracingEnabledFromCommandLine() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableBackgroundTracing);
}

}  // namespace tracing
