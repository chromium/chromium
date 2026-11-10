// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include <fstream>
#include <memory>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/debugger.h"
#include "base/debug/dump_without_crashing.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/logging_chrome.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chromeos/ash/components/logging/log_file_util.h"
#include "chromeos/ash/components/logging/session_log.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <initguid.h>

#include "base/logging_win.h"
#include "base/process/process_info.h"
#include "base/syslog_logging.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_handle_util.h"
#include "chrome/common/win/eventlog_messages.h"
#include "chrome/install_static/install_details.h"
#include "sandbox/policy/switches.h"
#endif

namespace logging {
namespace {

// When true, this means that error dialogs should not be shown.
bool dialogs_are_suppressed_ = false;
ScopedLogAssertHandler* assert_handler_ = nullptr;

// This should be true for exactly the period between the end of
// InitChromeLogging() and the beginning of CleanupChromeLogging().
bool chrome_logging_initialized_ = false;

// Set if we called InitChromeLogging() but failed to initialize.
bool chrome_logging_failed_ = false;

// This should be true for exactly the period between the end of
// InitChromeLogging() and the beginning of CleanupChromeLogging().
bool chrome_logging_redirected_ = false;

#if BUILDFLAG(IS_WIN)
// {7FE69228-633E-4f06-80C1-527FEA23E3A7}
const GUID kChromeTraceProviderName = {
    0x7fe69228,
    0x633e,
    0x4f06,
    {0x80, 0xc1, 0x52, 0x7f, 0xea, 0x23, 0xe3, 0xa7}};
#endif

// Assertion handler for logging errors that occur when dialogs are
// silenced.  To record a new error, pass the log string associated
// with that error in the str parameter.
NOINLINE void SilentRuntimeAssertHandler(const char* file,
                                         int line,
                                         std::string_view message,
                                         std::string_view stack_trace) {
  base::debug::BreakDebugger();
}

// Suppresses error/assertion dialogs and enables the logging of
// those errors into silenced_errors_.
void SuppressDialogs() {
  if (dialogs_are_suppressed_)
    return;

  assert_handler_ = new ScopedLogAssertHandler(
      base::BindRepeating(SilentRuntimeAssertHandler));

#if BUILDFLAG(IS_WIN)
  UINT new_flags =
      SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX;
  // Preserve existing error mode, as discussed at http://t/dmea
  UINT existing_flags = SetErrorMode(new_flags);
  SetErrorMode(existing_flags | new_flags);
#endif

  dialogs_are_suppressed_ = true;
}

#if BUILDFLAG(IS_WIN)
base::win::ScopedHandle GetLogInheritedHandle(
    const base::CommandLine& command_line) {
  auto handle_str = command_line.GetSwitchValueNative(switches::kLogFile);
  uint32_t handle_value = 0;
  if (!base::StringToUint(handle_str, &handle_value)) {
    return base::win::ScopedHandle();
  }
  // Duplicate the handle from the command line so that different things can
  // init logging. This means the handle from the parent is never closed, but
  // there will only be one of these in the process.
  HANDLE log_handle = nullptr;
  if (!::DuplicateHandle(GetCurrentProcess(),
                         base::win::Uint32ToHandle(handle_value),
                         GetCurrentProcess(), &log_handle, 0,
                         /*bInheritHandle=*/FALSE, DUPLICATE_SAME_ACCESS)) {
    return base::win::ScopedHandle();
  }
  // Transfer ownership to the caller.
  return base::win::ScopedHandle(log_handle);
}
#endif

// `filename_is_handle`, will be set to `true` if the log-file switch contains
// an inherited handle value rather than a filepath, and `false` otherwise.
LoggingDestination LoggingDestFromCommandLine(
    const base::CommandLine& command_line,
    bool& filename_is_handle) {
  filename_is_handle = false;
#if defined(NDEBUG)
  // In Release builds, log only to the log file.
  const LoggingDestination kDefaultLoggingMode = LOG_TO_FILE;
#else
  // In Debug builds log to all destinations, for ease of discovery.
  const LoggingDestination kDefaultLoggingMode = LOG_TO_ALL;
#endif

#if BUILDFLAG(CHROME_ENABLE_LOGGING_BY_DEFAULT)
  bool enable_logging = true;
  const char* const kInvertLoggingSwitch = switches::kDisableLogging;
#else
  bool enable_logging = false;
  const char* const kInvertLoggingSwitch = switches::kEnableLogging;
#endif

  if (command_line.HasSwitch(kInvertLoggingSwitch))
    enable_logging = !enable_logging;

  if (!enable_logging)
    return LOG_NONE;
  if (command_line.HasSwitch(switches::kEnableLogging)) {
    std::string logging_destination =
        command_line.GetSwitchValueASCII(switches::kEnableLogging);
    // Let --enable-logging=stderr force only stderr, particularly useful for
    // non-debug builds which otherwise default to LOG_TO_FILE.
    if (logging_destination == "stderr") {
      return LOG_TO_STDERR;
    }
    // Use --enable-logging=system to write to the system debug log, such as
    // Console.app / log(1) on Mac.
    if (logging_destination == "system") {
      return LOG_TO_SYSTEM_DEBUG_LOG;
    }
#if BUILDFLAG(IS_WIN)
    if (logging_destination == "handle" &&
        command_line.HasSwitch(switches::kProcessType) &&
        command_line.HasSwitch(switches::kLogFile)) {
      // Child processes can log to a handle duplicated from the parent, and
      // provided in the log-file switch value.
      filename_is_handle = true;
      return kDefaultLoggingMode | LOG_TO_FILE;
    }
#endif  // BUILDFLAG(IS_WIN)
    if (logging_destination != "") {
      // The browser process should not be called with --enable-logging=handle.
      LOG(ERROR) << "Invalid logging destination: " << logging_destination;
      return kDefaultLoggingMode;
    }
#if BUILDFLAG(IS_WIN)
    if (command_line.HasSwitch(switches::kProcessType) &&
        !command_line.HasSwitch(sandbox::policy::switches::kNoSandbox)) {
      // Sandboxed processes cannot open log files so skip if provided.
      return kDefaultLoggingMode & ~LOG_TO_FILE;
    }
#endif
  }
  return kDefaultLoggingMode;
}

}  // anonymous namespace

void InitChromeLogging(const base::CommandLine& command_line,
                       OldFileDeletionState delete_old_log_file) {
  DCHECK(!chrome_logging_initialized_)
      << "Attempted to initialize logging when it was already initialized.";
  bool filename_is_handle = false;
  LoggingDestination logging_dest =
      LoggingDestFromCommandLine(command_line, filename_is_handle);
  LogLockingState log_locking_state = LOCK_LOG_FILE;
  base::FilePath log_path;
#if BUILDFLAG(IS_CHROMEOS)
  base::FilePath target_path;
#endif
#if BUILDFLAG(IS_WIN)
  base::win::ScopedHandle log_handle;
#endif

  if (logging_dest & LOG_TO_FILE) {
    if (filename_is_handle) {
#if BUILDFLAG(IS_WIN)
      // Child processes on Windows are provided a file handle if logging is
      // enabled as sandboxed processes cannot open files.
      log_handle = GetLogInheritedHandle(command_line);
      if (!log_handle.is_valid()) {
        DLOG(ERROR) << "Unable to initialize logging from handle.";
        chrome_logging_failed_ = true;
        return;
      }
#endif
    } else {
      log_path = GetLogFileName(command_line);

#if BUILDFLAG(IS_CHROMEOS)
      // For BWSI (Incognito) logins, we want to put the logs in the user
      // profile directory that is created for the temporary session instead
      // of in the system log directory, for privacy reasons.
      if (command_line.HasSwitch(ash::switches::kGuestSession)) {
        log_path = ash::GetSessionLogDir().Append(log_path.BaseName());
      }

      // Prepares a log file.  We rotate the previous log file and prepare a new
      // log file if we've been asked to delete the old log, since that
      // indicates the start of a new session.
      target_path = ash::SetUpLogFile(
          log_path, delete_old_log_file == DELETE_OLD_LOG_FILE);

      // Because ChromeOS manages the move to a new session by redirecting
      // the link, it shouldn't remove the old file in the logging code,
      // since that will remove the newly created link instead.
      delete_old_log_file = APPEND_TO_OLD_LOG_FILE;
#endif  // BUILDFLAG(IS_CHROMEOS)
    }
  } else {
    log_locking_state = DONT_LOCK_LOG_FILE;
  }

  LoggingSettings settings;
  settings.logging_dest = logging_dest;
  if (!log_path.empty()) {
    settings.log_file_path = log_path.value().c_str();
  }
#if BUILDFLAG(IS_WIN)
  // Avoid initializing with INVALID_HANDLE_VALUE.
  // This handle is owned by the logging framework and is closed when the
  // process exits.
  // TODO(crbug.com/328285906) Use a ScopedHandle in logging settings.
  settings.log_file = log_handle.is_valid() ? log_handle.release() : nullptr;
#endif
  settings.lock_log = log_locking_state;
  settings.delete_old = delete_old_log_file;
  bool success = InitLogging(settings);

#if BUILDFLAG(IS_CHROMEOS)
  if (!success) {
    DPLOG(ERROR) << "Unable to initialize logging to " << log_path.value()
                 << " (which should be a link to " << target_path.value()
                 << ")";
    ash::RemoveSymlinkAndLog(log_path, target_path);
    chrome_logging_failed_ = true;
    return;
  }
#else   // BUILDFLAG(IS_CHROMEOS)
  if (!success) {
    DPLOG(ERROR) << "Unable to initialize logging to " << log_path.value();
    chrome_logging_failed_ = true;
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // We call running in unattended mode "headless", and allow headless mode to
  // be configured either by the Environment Variable or by the Command Line
  // Switch. This is for automated test purposes.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  const bool is_headless = env->HasVar(env_vars::kHeadless) ||
                           command_line.HasSwitch(switches::kNoErrorDialogs);

  // Show fatal log messages in a dialog in debug builds when not headless.
  if (!is_headless)
    SetShowErrorDialogs(true);

  // we want process and thread IDs because we have a lot of things running
  SetLogItems(true,    // enable_process_id
              true,    // enable_thread_id
              true,    // enable_timestamp
              false);  // enable_tickcount

  // Suppress system error dialogs when headless.
  if (is_headless)
    SuppressDialogs();

  // Use a minimum log level if the command line asks for one. Ignore this
  // switch if there's vlog level switch present too (as both of these switches
  // refer to the same underlying log level, and the vlog level switch has
  // already been processed inside InitLogging). If there is neither
  // log level nor vlog level specified, then just leave the default level
  // (INFO).
  if (command_line.HasSwitch(switches::kLoggingLevel) &&
      GetMinLogLevel() >= 0) {
    std::string log_level =
        command_line.GetSwitchValueASCII(switches::kLoggingLevel);
    int level = 0;
    if (base::StringToInt(log_level, &level) && level >= 0 &&
        level < LOGGING_NUM_SEVERITIES) {
      SetMinLogLevel(level);
    } else {
      DLOG(WARNING) << "Bad log level: " << log_level;
    }
  }

#if BUILDFLAG(IS_WIN)
  // Enable trace control and transport through event tracing for Windows.
  LogEventProvider::Initialize(kChromeTraceProviderName);

  // Enable logging to the Windows Event Log.
  SetEventSource(base::WideToASCII(
                     install_static::InstallDetails::Get().install_full_name()),
                 BROWSER_CATEGORY, MSG_LOG_MESSAGE);
#endif

  base::StatisticsRecorder::InitLogOnShutdown();

  chrome_logging_initialized_ = true;
}

// This is a no-op, but we'll keep it around in case
// we need to do more cleanup in the future.
void CleanupChromeLogging() {
  if (chrome_logging_failed_)
    return;  // We failed to initiailize logging, no cleanup.

  // Logging was not initialized, no cleanup required. This is happening with
  // the Chrome early exit error paths (i.e Process Singleton).
  if (!chrome_logging_initialized_)
    return;

  CloseLogFile();

  chrome_logging_initialized_ = false;
  chrome_logging_redirected_ = false;
}

base::FilePath GetLogFileName(const base::CommandLine& command_line) {
  // Try the command line.
  auto filename = command_line.GetSwitchValueNative(switches::kLogFile);
  // Try the environment.
  if (filename.empty()) {
    std::optional<std::string> env_filename =
        base::Environment::Create()->GetVar(env_vars::kLogFileName);
#if BUILDFLAG(IS_WIN)
    filename = base::UTF8ToWide(env_filename.value_or(""));
#else
    filename = env_filename.value_or("");
#endif  // BUILDFLAG(IS_WIN)
  }

  if (!filename.empty()) {
    base::FilePath candidate_path(filename);
#if BUILDFLAG(IS_WIN)
    // Windows requires an absolute path for the --log-file switch. Windows
    // cannot log to the current directory as it cds() to the exe's directory
    // earlier than this function runs.
    candidate_path = candidate_path.NormalizePathSeparators();
    if (candidate_path.IsAbsolute()) {
      return candidate_path;
    } else {
      PLOG(ERROR) << "Invalid logging destination: " << filename;
    }
#else
    return candidate_path;
#endif  // BUILDFLAG(IS_WIN)
  }

  // If command line and environment do not provide a log file we can use,
  // fallback to the default.
  const base::FilePath log_filename(FILE_PATH_LITERAL("chrome_debug.log"));
  base::FilePath log_path;

  if (base::PathService::Get(chrome::DIR_LOGS, &log_path)) {
    log_path = log_path.Append(log_filename);
    return log_path;
  } else {
#if BUILDFLAG(IS_WIN)
    // On Windows we cannot use a non-absolute path so we cannot provide a file.
    return base::FilePath();
#else
    // Error with path service, just use the default in our current directory.
    return log_filename;
#endif  // BUILDFLAG(IS_WIN)
  }
}

bool DialogsAreSuppressed() {
  return dialogs_are_suppressed_;
}

}  // namespace logging
