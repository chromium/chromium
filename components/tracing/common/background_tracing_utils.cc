// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_utils.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/snappy/src/snappy.h"

namespace tracing {

BASE_FEATURE(kTracingTriggers,
             "TracingTriggers",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFieldTracing, "FieldTracing", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPresetTracing,
             "PresetTracing",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

const base::FeatureParam<std::string> kTracingTriggerRulesConfig{
    &kTracingTriggers, "config", ""};
const base::FeatureParam<std::string> kFieldTracingConfig{&kFieldTracing,
                                                          "config", ""};
const base::FeatureParam<bool> kFieldTracingAnonymized{&kFieldTracing,
                                                       "anonymized", true};
const base::FeatureParam<bool> kFieldTracingForceUploads{
    &kFieldTracing, "force_uploads", false};
const base::FeatureParam<size_t> kFieldTracingUploadLimitKb{
    &kFieldTracing, "upload_limit_kb", 0};
const base::FeatureParam<bool> kStartupFieldTracing{&kFieldTracing, "startup",
                                                    false};
const base::FeatureParam<std::string> kPresetTracingConfig{&kPresetTracing,
                                                           "config", ""};

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

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
ParseSerializedTracingConfig(const base::span<const uint8_t>& config_bytes) {
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
ParseEncodedTracingConfig(const std::string& config_string) {
  std::string serialized_config;
  if (!base::Base64Decode(config_string, &serialized_config)) {
    return std::nullopt;
  }

  // `serialized_config` may optionally be compressed.
  std::string decompressed_config;
  if (!snappy::Uncompress(serialized_config.data(), serialized_config.size(),
                          &decompressed_config)) {
    return ParseSerializedTracingConfig(base::as_byte_span(serialized_config));
  }

  return ParseSerializedTracingConfig(base::as_byte_span(decompressed_config));
}

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
GetFieldTracingConfig() {
  if (!base::FeatureList::IsEnabled(kFieldTracing)) {
    return std::nullopt;
  }
  return ParseEncodedTracingConfig(kFieldTracingConfig.Get());
}

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
GetPresetTracingConfig() {
  if (!base::FeatureList::IsEnabled(kPresetTracing)) {
    return std::nullopt;
  }
  return ParseEncodedTracingConfig(kPresetTracingConfig.Get());
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

}  // namespace

void RecordDisallowedMetric(TracingFinalizationDisallowedReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Tracing.Background.FinalizationDisallowedReason",
                            reason);
}

bool SetupBackgroundTracingFromProtoConfigFile(
    const base::FilePath& config_file) {
  std::optional<perfetto::protos::gen::ChromeFieldTracingConfig> config;
  std::string config_text;
  if (base::ReadFileToString(config_file, &config_text) &&
      !config_text.empty()) {
    if (base::FilePath::CompareEqualIgnoreCase(config_file.Extension(),
                                               FILE_PATH_LITERAL(".pb"))) {
      config = ParseSerializedTracingConfig(base::as_byte_span(config_text));
    } else {
      config = ParseEncodedTracingConfig(config_text);
    }
  }
  if (!config) {
    LOG(ERROR) << "Failed to read field tracing config file "
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

  switch (GetBackgroundTracingSetupMode()) {
    case BackgroundTracingSetupMode::kDisabledInvalidCommandLine:
      return false;
    case BackgroundTracingSetupMode::kFromProtoConfigFile:
      return SetupBackgroundTracingFromProtoConfigFile(
          command_line->GetSwitchValuePath(switches::kEnableBackgroundTracing));
    case BackgroundTracingSetupMode::kFromFieldTrial:
      return false;
  }
}

bool SetupPresetTracingFromFieldTrial() {
  if (GetBackgroundTracingSetupMode() !=
      BackgroundTracingSetupMode::kFromFieldTrial) {
    return false;
  }

  auto& manager = content::BackgroundTracingManager::GetInstance();
  auto field_tracing_config = tracing::GetPresetTracingConfig();
  if (field_tracing_config) {
    content::BackgroundTracingManager::DataFiltering data_filtering =
        tracing::BackgroundTracingStateManager::GetInstance()
                .privacy_filter_enabled()
            ? content::BackgroundTracingManager::ANONYMIZE_DATA
            : content::BackgroundTracingManager::NO_DATA_FILTERING;
    manager.AddPresetScenarios(std::move(*field_tracing_config),
                               data_filtering);
    const auto& enabled_scenarios =
        tracing::BackgroundTracingStateManager::GetInstance()
            .enabled_scenarios();
    if (!enabled_scenarios.empty()) {
      return manager.SetEnabledScenarios(enabled_scenarios);
    }
    return true;
  }
  return false;
}

bool SetupSystemTracingFromFieldTrial() {
  if (tracing::GetBackgroundTracingSetupMode() !=
      BackgroundTracingSetupMode::kFromFieldTrial) {
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
  if (GetBackgroundTracingSetupMode() !=
      BackgroundTracingSetupMode::kFromFieldTrial) {
    return false;
  }

  bool is_local_scenario = false;
  if (tracing::HasBackgroundTracingOutputPath()) {
    is_local_scenario = true;
    if (!tracing::SetBackgroundTracingOutputPath()) {
      return false;
    }
  } else if (!kFieldTracingAnonymized.Get()) {
    is_local_scenario = true;
  }

  auto& manager = content::BackgroundTracingManager::GetInstance();
  auto field_tracing_config = tracing::GetFieldTracingConfig();
  if (!field_tracing_config) {
    return false;
  }

  if (is_local_scenario) {
    content::BackgroundTracingManager::DataFiltering data_filtering =
        tracing::BackgroundTracingStateManager::GetInstance()
                .privacy_filter_enabled()
            ? content::BackgroundTracingManager::ANONYMIZE_DATA
            : content::BackgroundTracingManager::NO_DATA_FILTERING;
    auto enabled_scenarios = manager.AddPresetScenarios(
        std::move(*field_tracing_config), data_filtering);
    return manager.SetEnabledScenarios(enabled_scenarios);
  }
  return manager.InitializeFieldScenarios(
      std::move(*field_tracing_config),
      content::BackgroundTracingManager::ANONYMIZE_DATA,
      kFieldTracingForceUploads.Get(), kFieldTracingUploadLimitKb.Get());
}

bool HasBackgroundTracingOutputPath() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kBackgroundTracingOutputPath);
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

BackgroundTracingSetupMode GetBackgroundTracingSetupMode() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableBackgroundTracing)) {
    return BackgroundTracingSetupMode::kFromFieldTrial;
  }

  if (command_line->HasSwitch(switches::kEnableBackgroundTracing) &&
      command_line->GetSwitchValueNative(switches::kEnableBackgroundTracing)
          .empty()) {
    LOG(ERROR) << "--enable-background-tracing needs a config file path";
    return BackgroundTracingSetupMode::kDisabledInvalidCommandLine;
  }

  if (command_line->HasSwitch(switches::kEnableBackgroundTracing)) {
    return BackgroundTracingSetupMode::kFromProtoConfigFile;
  }
  return BackgroundTracingSetupMode::kDisabledInvalidCommandLine;
}

bool ShouldTraceStartup() {
  return kStartupFieldTracing.Get();
}

}  // namespace tracing
