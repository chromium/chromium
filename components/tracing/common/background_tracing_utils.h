// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_UTILS_H_
#define COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_UTILS_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/scenario_config.gen.h"

namespace tracing {

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS) BASE_DECLARE_FEATURE(kFieldTracing);
COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
BASE_DECLARE_FEATURE(kTracingTriggers);

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "TracingFinalizationDisallowedReason" in
// src/tools/metrics/histograms/enums.xml.
enum class TracingFinalizationDisallowedReason {
  kIncognitoLaunched = 0,
  //  kProfileNotLoaded = 1, Obsolete
  //  kCrashMetricsNotLoaded = 2, Obsolete
  //  kLastSessionCrashed = 3, Obsolete
  //  kMetricsReportingDisabled = 4, Obsolete
  //  kTraceUploadedRecently = 5, Obsolete
  //  kLastTracingSessionDidNotEnd = 6, Obsolete as of Nov'2024.
  kMaxValue = kIncognitoLaunched
};

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
void RecordDisallowedMetric(TracingFinalizationDisallowedReason reason);

enum class BackgroundTracingSetupMode {
  // Background tracing config comes from a field trial.
  kFromFieldTrial,

  // Background tracing config comes from a proto config file passed on
  // the command-line (for local testing).
  kFromProtoConfigFile,

  // Background tracing is disabled due to invalid command-line flags.
  kDisabledInvalidCommandLine,
};

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupBackgroundTracingFromProtoConfigFile(
    const base::FilePath& config_file);

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupBackgroundTracingFromCommandLine();

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupPresetTracingFromFieldTrial();

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupFieldTracingFromFieldTrial();

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupSystemTracingFromFieldTrial();

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetBackgroundTracingOutputPath();

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool HasBackgroundTracingOutputPath();

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
BackgroundTracingSetupMode GetBackgroundTracingSetupMode();

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool ShouldTraceStartup();

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_UTILS_H_
