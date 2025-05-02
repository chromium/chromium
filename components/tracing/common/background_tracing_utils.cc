// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_utils.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/tracing_scenarios_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_thread.h"

namespace tracing {
namespace {

bool BlockingWriteTraceToFile(const base::FilePath& output_file,
                              std::string file_contents) {
  if (base::WriteFile(output_file, file_contents)) {
    LOG(ERROR) << "Background trace written to "
               << output_file.LossyDisplayName();
    return true;
  }
  LOG(ERROR) << "Failed to write background trace to "
             << output_file.LossyDisplayName();
  return false;
}

void WriteTraceToFile(
    const base::FilePath& output_path,
    const std::string& file_name,
    std::string file_contents,
    content::BackgroundTracingManager::FinishedProcessingCallback
        done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::FilePath output_file = output_path.AppendASCII(file_name);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&BlockingWriteTraceToFile, output_file,
                     std::move(file_contents)),
      std::move(done_callback));
}
}

bool SetupBackgroundTracingFromProtoConfigFile(
    const base::FilePath& config_file) {
  std::optional<perfetto::protos::gen::ChromeFieldTracingConfig> config;
  std::string config_text;
  if (base::ReadFileToString(config_file, &config_text) &&
      !config_text.empty()) {
    if (base::FilePath::CompareEqualIgnoreCase(config_file.Extension(),
                                               FILE_PATH_LITERAL(".pb"))) {
      config = tracing::ParseSerializedTracingScenariosConfig(
          base::as_byte_span(config_text));
    } else {
      config = tracing::ParseEncodedTracingScenariosConfig(config_text);
    }
  } else {
    LOG(ERROR) << "Failed to read field tracing config file "
               << config_file.value() << ".";
    return false;
  }

  if (!config) {
    LOG(ERROR) << "Failed to parse field tracing config file "
               << config_file.value() << "."
               << "Make sure to provide a proto (.pb) or base64 encoded (.txt)"
               << " file that contains scenarios config.";
    return false;
  }

  // NO_DATA_FILTERING is set because the trace is saved to a local output file
  // instead of being uploaded to a metrics server, so there are no PII
  // concerns.
  auto scenarios =
      content::BackgroundTracingManager::GetInstance().AddPresetScenarios(
          std::move(*config),
          content::BackgroundTracingManager::NO_DATA_FILTERING);

  return content::BackgroundTracingManager::GetInstance().SetEnabledScenarios(
      scenarios);
}

bool SetupBackgroundTracingFromCommandLine() {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  if (tracing::HasBackgroundTracingOutputPath() &&
      !tracing::SetBackgroundTracingOutputPath()) {
    return false;
  }

  if (!IsBackgroundTracingEnabledFromCommandLine()) {
    return false;
  }

  if (command_line->GetSwitchValueNative(switches::kEnableBackgroundTracing)
          .empty()) {
    LOG(ERROR) << "--enable-background-tracing needs a config file path";
    return false;
  }
  return SetupBackgroundTracingFromProtoConfigFile(
      command_line->GetSwitchValuePath(switches::kEnableBackgroundTracing));
}

bool SetupPresetTracingFromFieldTrial() {
  if (IsBackgroundTracingEnabledFromCommandLine()) {
    return false;
  }

  auto& config = BackgroundTracingStateManager::GetInstance();
  const auto& enabled_scenarios = config.enabled_scenarios();
  if (enabled_scenarios.empty()) {
    return false;
  }
  auto& manager = content::BackgroundTracingManager::GetInstance();
  auto tracing_scenarios_config = GetPresetTracingScenariosConfig();
  if (tracing_scenarios_config) {
    content::BackgroundTracingManager::DataFiltering data_filtering =
        config.privacy_filter_enabled()
            ? content::BackgroundTracingManager::ANONYMIZE_DATA
            : content::BackgroundTracingManager::NO_DATA_FILTERING;
    manager.AddPresetScenarios(std::move(*tracing_scenarios_config),
                               data_filtering);
    return manager.SetEnabledScenarios(enabled_scenarios);
  }
  return false;
}

bool SetupSystemTracingFromFieldTrial() {
  if (IsBackgroundTracingEnabledFromCommandLine()) {
    return false;
  }

  auto& manager = content::BackgroundTracingManager::GetInstance();
  auto trigger_config = tracing::GetTracingTriggerRulesConfig();
  if (!trigger_config) {
    return false;
  }
  return manager.InitializePerfettoTriggerRules(std::move(*trigger_config));
}

bool SetupFieldTracingFromFieldTrial() {
  if (IsBackgroundTracingEnabledFromCommandLine()) {
    return false;
  }

  bool local_scenarios = false;
  if (tracing::HasBackgroundTracingOutputPath()) {
    local_scenarios = true;
    if (!tracing::SetBackgroundTracingOutputPath()) {
      return false;
    }
  } else if (!kFieldTracingAnonymized.Get()) {
    local_scenarios = true;
  }

  auto& manager = content::BackgroundTracingManager::GetInstance();
  auto tracing_scenarios_config = tracing::GetFieldTracingScenariosConfig();
  if (!tracing_scenarios_config) {
    return false;
  }

  if (local_scenarios) {
    auto& config = BackgroundTracingStateManager::GetInstance();
    content::BackgroundTracingManager::DataFiltering data_filtering =
        config.privacy_filter_enabled()
            ? content::BackgroundTracingManager::ANONYMIZE_DATA
            : content::BackgroundTracingManager::NO_DATA_FILTERING;
    auto scenarios = manager.AddPresetScenarios(
        std::move(*tracing_scenarios_config), data_filtering);
    if (config.enabled_scenarios().empty()) {
      return manager.SetEnabledScenarios(scenarios);
    }
    return false;
  }
  return manager.InitializeFieldScenarios(
      std::move(*tracing_scenarios_config),
      content::BackgroundTracingManager::ANONYMIZE_DATA,
      kFieldTracingForceUploads.Get(), kFieldTracingUploadLimitKb.Get());
}

bool SetBackgroundTracingOutputPath() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->GetSwitchValuePath(switches::kBackgroundTracingOutputPath)
          .empty()) {
    LOG(ERROR) << "--background-tracing-output-path needs an output path";
    return false;
  }
  auto output_path =
      command_line->GetSwitchValuePath(switches::kBackgroundTracingOutputPath);

  auto receive_callback = base::BindRepeating(&WriteTraceToFile, output_path);
  content::BackgroundTracingManager::GetInstance().SetReceiveCallback(
      std::move(receive_callback));
  return true;
}

bool HasBackgroundTracingOutputPath() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kBackgroundTracingOutputPath);
}

}  // namespace tracing
