// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/test/chromedriver/logging.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <cmath>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/console_logger.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/command_listener_proxy.h"
#include "chrome/test/chromedriver/constants/version.h"
#include "chrome/test/chromedriver/devtools_events_logger.h"
#include "chrome/test/chromedriver/performance_logger.h"
#include "chrome/test/chromedriver/session.h"

#if BUILDFLAG(IS_POSIX)
#include <fcntl.h>
#include <unistd.h>
#elif BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

const char* GetPortProtectionMessage() {
  static std::string kPortProtectionMessage = base::StringPrintf(
      "Please see https://chromedriver.chromium.org/security-considerations "
      "for suggestions on keeping %s safe.",
      kChromeDriverProductShortName);
  return kPortProtectionMessage.c_str();
}

namespace {

Log::Level g_log_level = Log::kWarning;

int64_t g_start_time = 0;

bool readable_timestamp;

// Array indices are the Log::Level enum values.
const char* const kLevelToName[] = {
  "ALL",  // kAll
  "DEBUG",  // kDebug
  "INFO",  // kInfo
  "WARNING",  // kWarning
  "SEVERE",  // kError
  "OFF",  // kOff
};

const char* LevelToName(Log::Level level) {
  const int index = level - Log::kAll;
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index), std::size(kLevelToName));
  return kLevelToName[index];
}

struct LevelPair {
  const char* name;
  Log::Level level;
};

const LevelPair kNameToLevel[] = {
    {"ALL", Log::kAll},
    {"DEBUG", Log::kDebug},
    {"INFO", Log::kInfo},
    {"WARNING", Log::kWarning},
    {"SEVERE", Log::kError},
    {"OFF", Log::kOff},
};

Log::Level GetLevelFromSeverity(int severity) {
  switch (severity) {
    case logging::LOGGING_FATAL:
    case logging::LOGGING_ERROR:
      return Log::kError;
    case logging::LOGGING_WARNING:
      return Log::kWarning;
    case logging::LOGGING_INFO:
      return Log::kInfo;
    case logging::LOGGING_VERBOSE:
    default:
      return Log::kDebug;
  }
}

WebDriverLog* GetSessionLog() {
  Session* session = GetThreadLocalSession();
  if (!session)
    return nullptr;
  return session->driver_log.get();
}

bool InternalIsVLogOn(int vlog_level) {
  WebDriverLog* session_log = GetSessionLog();
  Log::Level session_level = session_log ? session_log->min_level() : Log::kOff;
  Log::Level level = g_log_level < session_level ? g_log_level : session_level;
  return GetLevelFromSeverity(vlog_level * -1) >= level;
}

bool HandleLogMessage(int severity,
                      const char* file,
                      int line,
                      size_t message_start,
                      const std::string& str) {
  Log::Level level = GetLevelFromSeverity(severity);
  std::string message = str.substr(message_start);

  if (level >= g_log_level) {
    const char* level_name = LevelToName(level);
    std::string entry;

    if (readable_timestamp) {
#if BUILDFLAG(IS_WIN)
      SYSTEMTIME local_time;
      GetLocalTime(&local_time);

      entry = base::StringPrintf(
          "[%02d-%02d-%04d %02d:%02d:%02d.%03d][%s]: %s",
          local_time.wMonth, local_time.wDay, local_time.wYear,
          local_time.wHour, local_time.wMinute, local_time.wSecond,
          local_time.wMilliseconds,
          level_name,
          message.c_str());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
      timeval tv;
      gettimeofday(&tv, nullptr);
      time_t t = tv.tv_sec;
      struct tm local_time;
      localtime_r(&t, &local_time);
      struct tm* tm_time = &local_time;

      entry = base::StringPrintf(
          "[%02d-%02d-%04d %02d:%02d:%02d.%06ld][%s]: %s",
          1 + tm_time->tm_mon, tm_time->tm_mday, 1900 + tm_time->tm_year,
          tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec,
          static_cast<long>(tv.tv_usec),
          level_name,
          message.c_str());
#else
#error Unsupported platform
#endif
    } else {
      entry = base::StringPrintf(
          "[%.3lf][%s]: %s",
          base::TimeDelta(base::TimeTicks::Now() -
                          base::TimeTicks::UnixEpoch())
              .InSecondsF(),
          level_name,
          message.c_str());
    }
    fprintf(stderr, "%s", entry.c_str());
    fflush(stderr);
  }

  WebDriverLog* session_log = GetSessionLog();
  if (session_log)
    session_log->AddEntry(level, message);

  return true;
}

}  // namespace

const char WebDriverLog::kBrowserType[] = "browser";
const char WebDriverLog::kDriverType[] = "driver";
const char WebDriverLog::kPerformanceType[] = "performance";
const char WebDriverLog::kDevToolsType[] = "devtools";

bool WebDriverLog::NameToLevel(const std::string& name, Log::Level* out_level) {
  for (size_t i = 0; i < std::size(kNameToLevel); ++i) {
    if (name == kNameToLevel[i].name) {
      *out_level = kNameToLevel[i].level;
      return true;
    }
  }
  return false;
}

WebDriverLog::WebDriverLog(const std::string& type, Log::Level min_level)
    : type_(type), min_level_(min_level), emptied_(true) {}

WebDriverLog::~WebDriverLog() {
  size_t sum = 0;
  for (const base::Value::List& batch : batches_of_entries_)
    sum += batch.size();
  VLOG(1) << "Log type '" << type_ << "' lost " << sum
          << " entries on destruction";
}

base::Value::List WebDriverLog::GetAndClearEntries() {
  if (batches_of_entries_.empty()) {
    emptied_ = true;
    return base::Value::List();
  } else {
    base::Value::List list = std::move(batches_of_entries_.front());
    batches_of_entries_.pop_front();
    emptied_ = false;
    return list;
  }
}

bool GetFirstErrorMessageFromList(const base::Value::List& list,
                                  std::string* message) {
  for (const auto& entry : list) {
    if (entry.is_dict()) {
      const base::Value::Dict& log_entry = entry.GetDict();
      const std::string* level = log_entry.FindString("level");
      if (!level || *level != kLevelToName[Log::kError])
        continue;

      if (const std::string* maybe_message = log_entry.FindString("message")) {
        *message = *maybe_message;
        return true;
      }
    }
  }
  return false;
}

std::string WebDriverLog::GetFirstErrorMessage() const {
  std::string message;
  for (const base::Value::List& list : batches_of_entries_)
    if (GetFirstErrorMessageFromList(list, &message))
      break;
  return message;
}

void WebDriverLog::AddEntryTimestamped(const base::Time& timestamp,
                                       Log::Level level,
                                       const std::string& source,
                                       const std::string& message) {
  if (level < min_level_)
    return;

  base::Value::Dict log_entry_dict;
  log_entry_dict.Set("timestamp",
                     std::trunc(timestamp.InMillisecondsFSinceUnixEpoch()));
  log_entry_dict.Set("level", LevelToName(level));
  if (!source.empty())
    log_entry_dict.Set("source", source);
  log_entry_dict.Set("message", message);
  if (batches_of_entries_.empty() ||
      batches_of_entries_.back().size() >= internal::kMaxReturnedEntries) {
    batches_of_entries_.push_back(base::Value::List());
  }
  batches_of_entries_.back().Append(std::move(log_entry_dict));
}

bool WebDriverLog::Emptied() const {
  return emptied_;
}

const std::string& WebDriverLog::type() const {
  return type_;
}

void WebDriverLog::set_min_level(Level min_level) {
  min_level_ = min_level;
}

Log::Level WebDriverLog::min_level() const {
  return min_level_;
}

bool InitLogging() {
  g_start_time = base::TimeTicks::Now().ToInternalValue();
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  if (cmd_line->HasSwitch("log-path")) {
    g_log_level = Log::kInfo;
    base::FilePath log_path = cmd_line->GetSwitchValuePath("log-path");

    const base::FilePath::CharType* log_mode = FILE_PATH_LITERAL("w");
    if (cmd_line->HasSwitch("append-log")) {
      log_mode = FILE_PATH_LITERAL("a");
    }
    if (cmd_line->HasSwitch("readable-timestamp")) {
      readable_timestamp = true;
    }
#if BUILDFLAG(IS_WIN)
    FILE* redir_stderr = _wfreopen(log_path.value().c_str(), log_mode, stderr);
#else
    FILE* redir_stderr = freopen(log_path.value().c_str(), log_mode, stderr);
#endif
    if (!redir_stderr) {
      printf("Failed to redirect stderr to log file.\n");
      return false;
    }
  }

  Log::truncate_logged_params = !cmd_line->HasSwitch("replayable");
  Log::is_vlog_on_func = &InternalIsVLogOn;

  int num_level_switches = 0;

  if (cmd_line->HasSwitch("silent")) {
    g_log_level = Log::kOff;
    num_level_switches++;
  }

  if (cmd_line->HasSwitch("verbose")) {
    g_log_level = Log::kAll;
    num_level_switches++;
  }

  if (cmd_line->HasSwitch("log-level")) {
    if (!WebDriverLog::NameToLevel(cmd_line->GetSwitchValueASCII("log-level"),
                                   &g_log_level)) {
      printf("Invalid --log-level value.\n");
      return false;
    }
    num_level_switches++;
  }

  if (num_level_switches > 1) {
    printf("Only one of --log-level, --verbose, or --silent is allowed.\n");
    return false;
  }

  // Turn on VLOG for chromedriver. This is parsed during logging::InitLogging.
  if (!cmd_line->HasSwitch("vmodule"))
    cmd_line->AppendSwitchASCII("vmodule", "*/chrome/test/chromedriver/*=3");

  logging::SetMinLogLevel(logging::LOGGING_WARNING);
  logging::SetLogItems(false,   // enable_process_id
                       false,   // enable_thread_id
                       false,   // enable_timestamp
                       false);  // enable_tickcount
  logging::SetLogMessageHandler(&HandleLogMessage);

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  return logging::InitLogging(logging_settings);
}

Status CreateLogs(
    const Capabilities& capabilities,
    const Session* session,
    std::vector<std::unique_ptr<WebDriverLog>>* out_logs,
    std::vector<std::unique_ptr<DevToolsEventListener>>* out_devtools_listeners,
    std::vector<std::unique_ptr<CommandListener>>* out_command_listeners) {
  std::vector<std::unique_ptr<WebDriverLog>> logs;
  std::vector<std::unique_ptr<DevToolsEventListener>> devtools_listeners;
  std::vector<std::unique_ptr<CommandListener>> command_listeners;
  Log::Level browser_log_level = Log::kWarning;
  const LoggingPrefs& prefs = capabilities.logging_prefs;

  for (auto iter = prefs.begin(); iter != prefs.end(); ++iter) {
    std::string type = iter->first;
    Log::Level level = iter->second;
    if (type == WebDriverLog::kPerformanceType) {
      if (level != Log::kOff) {
        logs.push_back(std::make_unique<WebDriverLog>(type, Log::kAll));
        devtools_listeners.push_back(std::make_unique<PerformanceLogger>(
            logs.back().get(), session, capabilities.perf_logging_prefs,
            base::Contains(capabilities.window_types,
                           WebViewInfo::kServiceWorker)));
        PerformanceLogger* perf_log =
            static_cast<PerformanceLogger*>(devtools_listeners.back().get());
        // We use a proxy for |perf_log|'s |CommandListener| interface.
        // Otherwise, |perf_log| would be owned by both session->chrome and
        // |session|, which would lead to memory errors on destruction.
        // session->chrome will own |perf_log|, and |session| will own |proxy|.
        // session->command_listeners (the proxy) will be destroyed first.
        command_listeners.push_back(
            std::make_unique<CommandListenerProxy>(perf_log));
      }
    } else if (type == WebDriverLog::kDevToolsType) {
      logs.push_back(std::make_unique<WebDriverLog>(type, Log::kAll));
      devtools_listeners.push_back(std::make_unique<DevToolsEventsLogger>(
          logs.back().get(), capabilities.devtools_events_logging_prefs));
    } else if (type == WebDriverLog::kBrowserType) {
      browser_log_level = level;
    } else if (type != WebDriverLog::kDriverType) {
      // Driver "should" ignore unrecognized log types, per Selenium tests.
      // For example the Java client passes the "client" log type in the caps,
      // which the server should never provide.
      LOG(WARNING) << "Ignoring unrecognized log type: " << type;
    }
  }
  // Create "browser" log -- should always exist.
  logs.push_back(std::make_unique<WebDriverLog>(WebDriverLog::kBrowserType,
                                                browser_log_level));
  // If the level is OFF, don't even bother listening for DevTools events.
  if (browser_log_level != Log::kOff)
    devtools_listeners.push_back(
        std::make_unique<ConsoleLogger>(logs.back().get()));

  out_logs->swap(logs);
  out_devtools_listeners->swap(devtools_listeners);
  out_command_listeners->swap(command_listeners);
  return Status(kOk);
}
