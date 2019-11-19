// Copyright (c) 2015 The Chromium Authors. All rights reserved.
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
#include "base/values.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"

#if defined(OS_ANDROID)
#include "base/android/early_trace_event_binding.h"
#endif

namespace tracing {

namespace {

// Maximum trace config file size that will be loaded, in bytes.
const size_t kTraceConfigFileSizeLimit = 64 * 1024;

// Trace config file path:
// - Android: /data/local/chrome-trace-config.json
// - Others: specified by --trace-config-file flag.
#if defined(OS_ANDROID)
const base::FilePath::CharType kAndroidTraceConfigFile[] =
    FILE_PATH_LITERAL("/data/local/chrome-trace-config.json");

const char kDefaultStartupCategories[] =
    "startup,browser,toplevel,disabled-by-default-toplevel.flow,ipc,disabled-"
    "by-default-ipc.flow,EarlyJava,cc,Java,navigation,loading,gpu,"
    "disabled-by-default-cpu_profiler,download_service,-*";
#else
const char kDefaultStartupCategories[] =
    "benchmark,toplevel,startup,disabled-by-default-file,disabled-by-default-"
    "toplevel.flow,disabled-by-default-ipc.flow,download_service,-*";
#endif

// String parameters that can be used to parse the trace config file content.
const char kTraceConfigParam[] = "trace_config";
const char kStartupDurationParam[] = "startup_duration";
const char kResultFileParam[] = "result_file";
const char kResultDirectoryParam[] = "result_directory";

}  // namespace

// static
TraceStartupConfig* TraceStartupConfig::GetInstance() {
  return base::Singleton<TraceStartupConfig, base::DefaultSingletonTraits<
                                                 TraceStartupConfig>>::get();
}

// static
base::trace_event::TraceConfig
TraceStartupConfig::GetDefaultBrowserStartupConfig() {
  base::trace_event::TraceConfig trace_config(
      kDefaultStartupCategories, base::trace_event::RECORD_UNTIL_FULL);
  // Filter only browser process events.
  base::trace_event::TraceConfig::ProcessFilterConfig process_config(
      {base::GetCurrentProcId()});
  // First 10k events at start are sufficient to debug startup traces.
  trace_config.SetTraceBufferSizeInEvents(10000);
  trace_config.SetProcessFilterConfig(process_config);
  return trace_config;
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
    DCHECK(IsEnabled() || IsUsingPerfettoOutput());
  } else if (EnableFromBackgroundTracing()) {
    DCHECK(IsEnabled());
    DCHECK(!IsTracingStartupForDuration());
    DCHECK_EQ(SessionOwner::kBackgroundTracing, session_owner_);
    CHECK(!ShouldTraceToResultFile());
  }
}

TraceStartupConfig::~TraceStartupConfig() = default;

bool TraceStartupConfig::IsEnabled() const {
  // TODO(oysteine): Support early startup tracing using Perfetto
  // output; right now the early startup tracing gets controlled
  // through the TracingController, and the Perfetto output is
  // using the Consumer Mojo interface; the two can't be used
  // together.
  return is_enabled_ && !IsUsingPerfettoOutput();
}

void TraceStartupConfig::SetDisabled() {
  is_enabled_ = false;
}

bool TraceStartupConfig::IsTracingStartupForDuration() const {
  return IsEnabled() && startup_duration_in_seconds_ > 0 &&
         session_owner_ == SessionOwner::kTracingController;
}

base::trace_event::TraceConfig TraceStartupConfig::GetTraceConfig() const {
  DCHECK(IsEnabled() || IsUsingPerfettoOutput());
  return trace_config_;
}

int TraceStartupConfig::GetStartupDuration() const {
  DCHECK(IsEnabled() || IsUsingPerfettoOutput());
  return startup_duration_in_seconds_;
}

bool TraceStartupConfig::ShouldTraceToResultFile() const {
  return IsEnabled() && should_trace_to_result_file_;
}

base::FilePath TraceStartupConfig::GetResultFile() const {
  DCHECK(IsEnabled());
  DCHECK(ShouldTraceToResultFile());
  return result_file_;
}

void TraceStartupConfig::OnTraceToResultFileFinished() {
  finished_writing_to_file_ = true;
}

bool TraceStartupConfig::IsUsingPerfettoOutput() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPerfettoOutputFile);
}

void TraceStartupConfig::SetBackgroundStartupTracingEnabled(bool enabled) {
#if defined(OS_ANDROID)
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

  // Startup duration can be used by along with perfetto-output-file flag.
  if (command_line->HasSwitch(switches::kTraceStartupDuration)) {
    std::string startup_duration_str =
        command_line->GetSwitchValueASCII(switches::kTraceStartupDuration);
    if (!startup_duration_str.empty() &&
        !base::StringToInt(startup_duration_str, &startup_duration_in_seconds_)) {
      DLOG(WARNING) << "Could not parse --" << switches::kTraceStartupDuration
                    << "=" << startup_duration_str << " defaulting to 5 (secs)";
      startup_duration_in_seconds_ = kDefaultStartupDurationInSeconds;
    }
  }

  if (!command_line->HasSwitch(switches::kTraceStartup))
    return false;

  trace_config_ = base::trace_event::TraceConfig(
      command_line->GetSwitchValueASCII(switches::kTraceStartup),
      command_line->GetSwitchValueASCII(switches::kTraceStartupRecordMode));

  result_file_ = command_line->GetSwitchValuePath(switches::kTraceStartupFile);

  is_enabled_ = true;
  should_trace_to_result_file_ = true;
  return true;
}

bool TraceStartupConfig::EnableFromConfigFile() {
#if defined(OS_ANDROID)
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
    should_trace_to_result_file_ = true;
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
  should_trace_to_result_file_ = is_enabled_;
  return is_enabled_;
}

bool TraceStartupConfig::EnableFromBackgroundTracing() {
  bool enabled = enable_background_tracing_for_testing_;
#if defined(OS_ANDROID)
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
  should_trace_to_result_file_ = false;
  // Set startup duration to 0 since background tracing config will configure
  // the durations later.
  startup_duration_in_seconds_ = 0;
  return true;
}

bool TraceStartupConfig::ParseTraceConfigFileContent(
    const std::string& content) {
  std::unique_ptr<base::Value> value(base::JSONReader::ReadDeprecated(content));
  if (!value || !value->is_dict())
    return false;

  std::unique_ptr<base::DictionaryValue> dict(
      static_cast<base::DictionaryValue*>(value.release()));

  base::DictionaryValue* trace_config_dict = nullptr;
  if (!dict->GetDictionary(kTraceConfigParam, &trace_config_dict))
    return false;

  trace_config_ = base::trace_event::TraceConfig(*trace_config_dict);

  if (!dict->GetInteger(kStartupDurationParam, &startup_duration_in_seconds_))
    startup_duration_in_seconds_ = 0;

  if (startup_duration_in_seconds_ < 0)
    startup_duration_in_seconds_ = 0;

  base::FilePath::StringType result_file_or_dir_str;
  if (dict->GetString(kResultFileParam, &result_file_or_dir_str))
    result_file_ = base::FilePath(result_file_or_dir_str);
  else if (dict->GetString(kResultDirectoryParam, &result_file_or_dir_str)) {
    result_file_ = base::FilePath(result_file_or_dir_str);
    // Java time to get an int instead of a double.
    result_file_ = result_file_.AppendASCII(
        base::NumberToString(base::Time::Now().ToJavaTime()) +
        "_chrometrace.log");
  }

  return true;
}

}  // namespace tracing
