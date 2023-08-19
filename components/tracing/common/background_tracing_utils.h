// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_UTILS_H_
#define COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_UTILS_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "content/public/browser/background_tracing_config.h"

namespace tracing {

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "TracingFinalizationDisallowedReason" in
// src/tools/metrics/histograms/enums.xml.
enum class TracingFinalizationDisallowedReason {
  kIncognitoLaunched = 0,
  kProfileNotLoaded = 1,
  kCrashMetricsNotLoaded = 2,
  kLastSessionCrashed = 3,
  kMetricsReportingDisabled = 4,
  kTraceUploadedRecently = 5,
  kLastTracingSessionDidNotEnd = 6,
  kMaxValue = kLastTracingSessionDidNotEnd
};

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
void RecordDisallowedMetric(TracingFinalizationDisallowedReason reason);

enum class BackgroundTracingSetupMode {
  // Background tracing config comes from a field trial.
  kFromFieldTrial,

  // Background tracing config comes from a field trial but the trace is written
  // into a local file (for local testing).
  kFromFieldTrialLocalOutput,

  // Background tracing config comes from a json config file passed on
  // the command-line (for local testing).
  kFromJsonConfigFile,

  // Background tracing config comes from a proto config file passed on
  // the command-line (for local testing).
  kFromProtoConfigFile,

  // Background tracing is disabled due to invalid command-line flags.
  kDisabledInvalidCommandLine,
};

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupBackgroundTracingWithOutputFile(
    std::unique_ptr<content::BackgroundTracingConfig> config,
    const base::FilePath& output_file);

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupBackgroundTracingFromJsonConfigFile(
    const base::FilePath& config_file,
    const base::FilePath& output_file);

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupBackgroundTracingFromProtoConfigFile(
    const base::FilePath& config_file,
    const base::FilePath& output_file);

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupBackgroundTracingFromCommandLine(const std::string& field_trial_name);

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
BackgroundTracingSetupMode GetBackgroundTracingSetupMode();

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_UTILS_H_
