// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_utils.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_thread.h"

namespace tracing {

namespace {

bool BlockingWriteTraceToFile(const base::FilePath& output_file,
                              std::unique_ptr<std::string> file_contents) {
  if (base::WriteFile(output_file, *file_contents)) {
    LOG(ERROR) << "Background trace written to "
               << output_file.LossyDisplayName();
    return true;
  }
  LOG(ERROR) << "Failed to write background trace to "
             << output_file.LossyDisplayName();
  return false;
}

void WriteTraceToFile(
    const base::FilePath& output_file,
    std::unique_ptr<std::string> file_contents,
    content::BackgroundTracingManager::FinishedProcessingCallback
        done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&BlockingWriteTraceToFile, output_file,
                     std::move(file_contents)),
      std::move(done_callback));
}

std::unique_ptr<content::BackgroundTracingConfig>
GetBackgroundTracingConfigFromFile(const base::FilePath& config_file) {
  std::string config_text;
  if (!base::ReadFileToString(config_file, &config_text) ||
      config_text.empty()) {
    LOG(ERROR) << "Failed to read background tracing config file "
               << config_file.value();
    return nullptr;
  }

  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      config_text, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!value_with_error.has_value()) {
    LOG(ERROR) << "Background tracing has incorrect config: "
               << value_with_error.error().message;
    return nullptr;
  }

  if (!value_with_error->is_dict()) {
    LOG(ERROR) << "Background tracing config is not a dict";
    return nullptr;
  }

  auto config = content::BackgroundTracingConfig::FromDict(
      std::move(*value_with_error).TakeDict());

  if (!config) {
    LOG(ERROR) << "Background tracing config dict has invalid contents";
    return nullptr;
  }

  return config;
}

}  // namespace

void RecordDisallowedMetric(TracingFinalizationDisallowedReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Tracing.Background.FinalizationDisallowedReason",
                            reason);
}

void SetupBackgroundTracingWithOutputFile(
    std::unique_ptr<content::BackgroundTracingConfig> config,
    const base::FilePath& output_file) {
  if (output_file.empty()) {
    return;
  }

  auto receive_callback = base::BindRepeating(&WriteTraceToFile, output_file);

  // NO_DATA_FILTERING is set because the trace is saved to a local output file
  // instead of being uploaded to a metrics server, so there are no PII
  // concerns.
  content::BackgroundTracingManager::GetInstance()
      .SetActiveScenarioWithReceiveCallback(
          std::move(config), std::move(receive_callback),
          content::BackgroundTracingManager::NO_DATA_FILTERING);
}

void SetupBackgroundTracingFromConfigFile(const base::FilePath& config_file,
                                          const base::FilePath& output_file) {
  std::unique_ptr<content::BackgroundTracingConfig> config =
      GetBackgroundTracingConfigFromFile(config_file);
  if (!config) {
    return;
  }

  SetupBackgroundTracingWithOutputFile(std::move(config), output_file);
}

bool SetupBackgroundTracingFromCommandLine(
    const std::string& field_trial_name) {
  auto& manager = content::BackgroundTracingManager::GetInstance();
  auto* command_line = base::CommandLine::ForCurrentProcess();

  switch (GetBackgroundTracingSetupMode()) {
    case BackgroundTracingSetupMode::kDisabledInvalidCommandLine:
      return false;
    case BackgroundTracingSetupMode::kFromConfigFile:
      SetupBackgroundTracingFromConfigFile(
          command_line->GetSwitchValuePath(switches::kEnableBackgroundTracing),
          command_line->GetSwitchValuePath(
              switches::kBackgroundTracingOutputFile));
      return true;
    case BackgroundTracingSetupMode::kFromFieldTrialLocalOutput:
      SetupBackgroundTracingWithOutputFile(
          manager.GetBackgroundTracingConfig(field_trial_name),
          command_line->GetSwitchValuePath(
              switches::kBackgroundTracingOutputFile));
      return true;
    case BackgroundTracingSetupMode::kFromFieldTrial:
      return false;
  }
}

BackgroundTracingSetupMode GetBackgroundTracingSetupMode() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableBackgroundTracing)) {
    if (!command_line->HasSwitch(switches::kBackgroundTracingOutputFile)) {
      return BackgroundTracingSetupMode::kFromFieldTrial;
    }

    if (command_line->GetSwitchValuePath(switches::kBackgroundTracingOutputFile)
            .empty()) {
      LOG(ERROR)
          << "--background-tracing-output-file needs an output file path";
      return BackgroundTracingSetupMode::kDisabledInvalidCommandLine;
    }

    return BackgroundTracingSetupMode::kFromFieldTrialLocalOutput;
  }

  if (command_line->GetSwitchValueNative(switches::kEnableBackgroundTracing)
          .empty()) {
    LOG(ERROR) << "--enable-background-tracing needs a config file path";
    return BackgroundTracingSetupMode::kDisabledInvalidCommandLine;
  }

  const auto output_file = command_line->GetSwitchValueNative(
      switches::kBackgroundTracingOutputFile);
  if (output_file.empty()) {
    LOG(ERROR) << "--background-tracing-output-file needs an output file path";
    return BackgroundTracingSetupMode::kDisabledInvalidCommandLine;
  }

  return BackgroundTracingSetupMode::kFromConfigFile;
}

}  // namespace tracing
