// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_TRACING_SCENARIOS_CONFIG_H_
#define COMPONENTS_TRACING_COMMON_TRACING_SCENARIOS_CONFIG_H_

#include <optional>

#include "base/containers/span.h"
#include "base/features.h"
#include "components/tracing/tracing_export.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/scenario_config.gen.h"

namespace tracing {

TRACING_EXPORT BASE_DECLARE_FEATURE(kFieldTracing);

TRACING_EXPORT
BASE_DECLARE_FEATURE(kTracingTriggers);

TRACING_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kFieldTracingAnonymized);
TRACING_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kFieldTracingForceUploads);
TRACING_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kStartupFieldTracing);
TRACING_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t, kFieldTracingUploadLimitKb);

TRACING_EXPORT
std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
ParseEncodedTracingScenariosConfig(const std::string& config_string);

TRACING_EXPORT
std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
ParseSerializedTracingScenariosConfig(
    const base::span<const uint8_t>& config_bytes);

TRACING_EXPORT
std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
GetPresetTracingScenariosConfig();

TRACING_EXPORT
std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
GetFieldTracingScenariosConfig();

TRACING_EXPORT
std::optional<perfetto::protos::gen::TracingTriggerRulesConfig>
GetTracingTriggerRulesConfig();

TRACING_EXPORT
bool IsBackgroundTracingEnabledFromCommandLine();

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_TRACING_SCENARIOS_CONFIG_H_
