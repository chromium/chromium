// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/trace_startup_config.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_log.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/early_trace_event_binding.h"
#endif

namespace tracing {

namespace {

// Maximum trace config file size that will be loaded, in bytes.
const size_t kTraceConfigFileSizeLimit = 64 * 1024;

// Trace config file path:
// - Android: /data/local/chrome-trace-config.json
// - Others: specified by --trace-config-file flag.
#if BUILDFLAG(IS_ANDROID)
const base::FilePath::CharType kAndroidTraceConfigFile[] =
    FILE_PATH_LITERAL("/data/local/chrome-trace-config.json");
#endif

// String parameters that can be used to parse the trace config file content.
const char kTraceConfigParam[] = "trace_config";
const char kStartupDurationParam[] = "startup_duration";
const char kResultFileParam[] = "result_file";
const char kResultDirectoryParam[] = "result_directory";

}  // namespace

// static
const char TraceStartupConfig::kDefaultStartupCategories[] =
#if BUILDFLAG(IS_ANDROID)
    "startup,browser,toplevel,toplevel.flow,ipc,EarlyJava,cc,Java,navigation,"
    "loading,gpu,ui,disabled-by-default-cpu_profiler,download_service,"
    "disabled-by-default-histogram_samples,"
    "disabled-by-default-user_action_samples,-*";
#else
    "benchmark,toplevel,startup,disabled-by-default-file,toplevel.flow,"
    "download_service,-*";
#endif

// static
TraceStartupConfig* TraceStartupConfig::GetInstance() {
  return base::Singleton<TraceStartupConfig, base::DefaultSingletonTraits<
                                                 TraceStartupConfig>>::get();
}

// static
base::trace_event::TraceConfig
TraceStartupConfig::GetDefaultBrowserStartupConfig() {
  return base::trace_event::TraceConfig(
      kDefaultStartupCategories, base::trace_event::RECORD_UNTIL_FULL);
}

TraceStartupConfig::TraceStartupConfig() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  const std::string value =
      command_line->GetSwitchValueASCII(switches::kTraceStartupOwner);
  if (value == "devtools") {
    session_owner_ = SessionOwner::kDevToolsTracingHandler;
  } else if (value == "system") {
    session_owner_ = SessionOwner::kSystemTracing;
  }

  if (EnableFromCommandLine()) {
    DCHECK(IsEnabled());
  } else if (EnableFromConfigFile()) {
    DCHECK(IsEnabled());
  } else if (EnableFromBackgroundTracing()) {
    DCHECK(IsEnabled());
    DCHECK(!IsTracingStartupForDuration());
    DCHECK_EQ(SessionOwner::kBackgroundTracing, session_owner_);
    CHECK(GetResultFile().empty());
  }
}

TraceStartupConfig::~TraceStartupConfig() = default;

bool TraceStartupConfig::IsEnabled() const {
  return is_enabled_;
}

void TraceStartupConfig::SetDisabled() {
  is_enabled_ = false;
}

bool TraceStartupConfig::IsTracingStartupForDuration() const {
  return IsEnabled() && startup_duration_in_seconds_ > 0 &&
         session_owner_ == SessionOwner::kTracingController;
}

base::trace_event::TraceConfig TraceStartupConfig::GetTraceConfig() const {
  DCHECK(IsEnabled());
  return trace_config_;
}

int TraceStartupConfig::GetStartupDuration() const {
  DCHECK(IsEnabled());
  return startup_duration_in_seconds_;
}

TraceStartupConfig::OutputFormat TraceStartupConfig::GetOutputFormat() const {
  DCHECK(IsEnabled());
  return output_format_;
}

base::FilePath TraceStartupConfig::GetResultFile() const {
  DCHECK(IsEnabled());
  return result_file_;
}

void TraceStartupConfig::SetBackgroundStartupTracingEnabled(bool enabled) {
#if BUILDFLAG(IS_ANDROID)
  base::android::SetBackgroundStartupTracingFlag(enabled);
#endif
}

TraceStartupConfig::SessionOwner TraceStartupConfig::GetSessionOwner() const {
  DCHECK(IsEnabled());
  return session_owner_;
}

bool TraceStartupConfig::AttemptAdoptBySessionOwner(SessionOwner owner) {
  if (IsEnabled() && GetSessionOwner() == owner && !session_adopted_) {
    // The session can only be adopted once.
    session_adopted_ = true;
    return true;
  }
  return false;
}

bool TraceStartupConfig::EnableFromCommandLine() {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  bool tracing_enabled_from_command_line =
      command_line->HasSwitch(switches::kTraceStartup) ||
      command_line->HasSwitch(switches::kEnableTracing);

  if (command_line->HasSwitch(switches::kTraceStartupDuration)) {
    std::string startup_duration_str =
        command_line->GetSwitchValueASCII(switches::kTraceStartupDuration);
    if (!startup_duration_str.empty() &&
        !base::StringToInt(startup_duration_str, &startup_duration_in_seconds_)) {
      DLOG(WARNING) << "Could not parse --" << switches::kTraceStartupDuration
                    << "=" << startup_duration_str << " defaulting to 5 (secs)";
      startup_duration_in_seconds_ = kDefaultStartupDurationInSeconds;
    }
  } else if (command_line->HasSwitch(switches::kEnableTracing)) {
    // For --enable-tracing, tracing should last until browser shutdown.
    startup_duration_in_seconds_ = 0;
  }

  if (command_line->HasSwitch(switches::kTraceStartupFormat)) {
    if (command_line->GetSwitchValueASCII(switches::kTraceStartupFormat) ==
        "json") {
      // Default is "proto", so switch to json only if the "json" string is
      // provided.
      output_format_ = OutputFormat::kLegacyJSON;
    }
  } else if (command_line->HasSwitch(switches::kEnableTracingFormat)) {
    if (command_line->GetSwitchValueASCII(switches::kEnableTracingFormat) ==
        "json") {
      output_format_ = OutputFormat::kLegacyJSON;
    }
  }

  // This check is intentionally performed after setting duration and output
  // format to ensure that setting them from the command-line takes effect for
  // config file-based tracing as well.
  if (!tracing_enabled_from_command_line)
    return false;

  std::string categories;
  if (command_line->HasSwitch(switches::kTraceStartup)) {
    categories = command_line->GetSwitchValueASCII(switches::kTraceStartup);
  } else {
    categories = command_line->GetSwitchValueASCII(switches::kEnableTracing);
  }

  trace_config_ = base::trace_event::TraceConfig(
      categories,
      command_line->GetSwitchValueASCII(switches::kTraceStartupRecordMode));

  if (trace_config_.IsCategoryGroupEnabled(
          base::trace_event::MemoryDumpManager::kTraceCategory)) {
    base::trace_event::TraceConfig::MemoryDumpConfig memory_config;
    memory_config.triggers.push_back(
        {10000, base::trace_event::MemoryDumpLevelOfDetail::kDetailed,
         base::trace_event::MemoryDumpType::kPeriodicInterval});
    memory_config.allowed_dump_modes.insert(
        base::trace_event::MemoryDumpLevelOfDetail::kDetailed);
    trace_config_.ResetMemoryDumpConfig(memory_config);
  }

  result_file_ = command_line->GetSwitchValuePath(switches::kTraceStartupFile);

  is_enabled_ = true;
  return true;
}

bool TraceStartupConfig::EnableFromConfigFile() {
#if BUILDFLAG(IS_ANDROID)
  base::FilePath trace_config_file(kAndroidTraceConfigFile);
#else
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kTraceConfigFile))
    return false;
  base::FilePath trace_config_file =
      command_line->GetSwitchValuePath(switches::kTraceConfigFile);
#endif

  if (trace_config_file.empty()) {
    is_enabled_ = true;
    DLOG(WARNING) << "Use default trace config.";
    return true;
  }

  if (!base::PathExists(trace_config_file)) {
    DLOG(WARNING) << "The trace config file does not exist.";
    return false;
  }

  std::string trace_config_file_content;
  if (!base::ReadFileToStringWithMaxSize(trace_config_file,
                                         &trace_config_file_content,
                                         kTraceConfigFileSizeLimit)) {
    DLOG(WARNING) << "Cannot read the trace config file correctly.";
    return false;
  }
  is_enabled_ = ParseTraceConfigFileContent(trace_config_file_content);
  if (!is_enabled_)
    DLOG(WARNING) << "Cannot parse the trace config file correctly.";
  return is_enabled_;
}

bool TraceStartupConfig::EnableFromBackgroundTracing() {
  bool enabled = enable_background_tracing_for_testing_;
#if BUILDFLAG(IS_ANDROID)
  // Tests can enable this value.
  enabled |= base::android::GetBackgroundStartupTracingFlag();
#else
  // TODO(ssid): Implement saving setting to preference for next startup.
#endif
  // Do not set the flag to false if it's not enabled unnecessarily.
  if (!enabled)
    return false;

  SetBackgroundStartupTracingEnabled(false);
  trace_config_ = GetDefaultBrowserStartupConfig();
  trace_config_.EnableArgumentFilter();

  is_enabled_ = true;
  session_owner_ = SessionOwner::kBackgroundTracing;
  // Set startup duration to 0 since background tracing config will configure
  // the durations later.
  startup_duration_in_seconds_ = 0;
  return true;
}

bool TraceStartupConfig::ParseTraceConfigFileContent(
    const std::string& content) {
  absl::optional<base::Value> value(base::JSONReader::Read(content));
  if (!value || !value->is_dict())
    return false;

  auto& dict = value->GetDict();

  auto* trace_config_dict = dict.FindDict(kTraceConfigParam);
  if (!trace_config_dict)
    return false;

  trace_config_ = base::trace_event::TraceConfig(std::move(*trace_config_dict));

  startup_duration_in_seconds_ =
      dict.FindInt(kStartupDurationParam).value_or(0);

  if (startup_duration_in_seconds_ < 0)
    startup_duration_in_seconds_ = 0;

  if (auto* result_file = dict.FindString(kResultFileParam)) {
    result_file_ = base::FilePath::FromUTF8Unsafe(*result_file);
  } else if (auto* result_dir = dict.FindString(kResultDirectoryParam)) {
    result_file_ = base::FilePath::FromUTF8Unsafe(*result_dir);
    // Java time to get an int instead of a double.
    result_file_ = result_file_.AppendASCII(
        base::NumberToString(base::Time::Now().ToJavaTime()) +
        "_chrometrace.log");
  }

  return true;
}

}  // namespace tracing
