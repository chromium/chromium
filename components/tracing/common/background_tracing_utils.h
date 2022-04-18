// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_UTILS_H_
#define COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_UTILS_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "content/public/browser/background_tracing_config.h"

namespace tracing {

enum class BackgroundTracingSetupMode {
  // Background tracing config comes from a field trial.
  kFromFieldTrial,

  // Background tracing config comes from a field trial but the trace is written
  // into a local file (for local testing).
  kFromFieldTrialLocalOutput,

  // Background tracing config comes from a config file passed on the
  // command-line (for local testing).
  kFromConfigFile,

  // Background tracing is disabled due to invalid command-line flags.
  kDisabledInvalidCommandLine,
};

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
void SetupBackgroundTracingWithOutputFile(
    std::unique_ptr<content::BackgroundTracingConfig> config,
    const base::FilePath& output_file);

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
void SetupBackgroundTracingFromConfigFile(const base::FilePath& config_file,
                                          const base::FilePath& output_file);

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
bool SetupBackgroundTracingFromCommandLine(const std::string& field_trial_name);

COMPONENT_EXPORT(BACKGROUND_TRACING_UTILS)
BackgroundTracingSetupMode GetBackgroundTracingSetupMode();

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_BACKGROUND_TRACING_UTILS_H_
