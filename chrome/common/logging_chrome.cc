// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "build/build_config.h"
// Need to include this before most other files because it defines
// IPC_MESSAGE_LOG_ENABLED. We need to use it to define
// IPC_MESSAGE_MACROS_LOG_ENABLED so render_messages.h will generate the
// ViewMsgLog et al. functions.
#include "ipc/ipc_buildflags.h"

// On Windows, the about:ipc dialog shows IPCs; on POSIX, we hook up a
// logger in this file.  (We implement about:ipc on Mac but implement
// the loggers here anyway).  We need to do this real early to be sure
// IPC_MESSAGE_MACROS_LOG_ENABLED doesn't get undefined.
#if BUILDFLAG(IS_POSIX) && BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#include "content/public/common/content_ipc_logging.h"
#define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger) \
  content::RegisterIPCLogger(msg_id, logger)
#include "chrome/common/all_messages.h"
#endif

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
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/logging_chrome.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_logging.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/i18n/time_formatting.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <initguid.h>

#include "base/logging_win.h"
#include "base/process/process_info.h"
#include "base/syslog_logging.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
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

// The directory on which we do rotation of log files instead of switching
// with symlink. Because this directory doesn't support symlinks and the logic
// doesn't work correctly.
#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kChronosHomeDir[] = "/home/chronos/user/";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
    // Let --enable-logging=stderr force only stderr, particularly useful for
    // non-debug builds where otherwise you can't get logs to stderr at all.
    std::string logging_destination =
        command_line.GetSwitchValueASCII(switches::kEnableLogging);
    if (logging_destination == "stderr") {
      return LOG_TO_SYSTEM_DEBUG_LOG | LOG_TO_STDERR;
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

LoggingDestination DetermineLoggingDestination(
    const base::CommandLine& command_line) {
  bool unused = false;
  return LoggingDestFromCommandLine(command_line, unused);
}

#if BUILDFLAG(IS_CHROMEOS)
bool RotateLogFile(const base::FilePath& target_path) {
  DCHECK(!target_path.empty());
  // If the old log file doesn't exist, do nothing.
  if (!base::PathExists(target_path)) {
    return true;
  }

  // Retrieve the creation time of the old log file.
  base::File::Info info;
  {
    // Opens a file, only if it exists.
    base::File fp(target_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!fp.IsValid() || !fp.GetInfo(&info)) {
      // On failure, keep using the same file.
      return false;
    }
  }

  // Generate the rotated log path name from the creation time.
  // (eg. "/home/chrome/user/log/chrome_220102-030405")
  base::Time timestamp = info.creation_time;
  base::FilePath rotated_path = GenerateTimestampedName(target_path, timestamp);

  // Rare case: if the target path already exists, generate the alternative by
  // incrementing the timestamp. This may happen when the Chrome restarts
  // multiple times in a second.
  while (base::PathExists(rotated_path)) {
    timestamp += base::Seconds(1);
    rotated_path = GenerateTimestampedName(target_path, timestamp);
  }

  // Rename the old log file: |target_path| => |rotated_path|.
  // We don't use |base::Move|, since we don't consider the inter-filesystem
  // move in this logic. The current logic depends on the fact that the ctime
  // won't be changed after rotation, but ctime may be changed on
  // inter-filesystem move.
  if (!base::ReplaceFile(target_path, rotated_path, nullptr)) {
    PLOG(ERROR) << "Failed to rotate the log files: " << target_path << " => "
                << rotated_path;
    return false;
  }

  return true;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
base::FilePath SetUpSymlinkIfNeeded(const base::FilePath& symlink_path,
                                    bool new_log) {
  DCHECK(!symlink_path.empty());
  // For backward compatibility, set up a .../chrome symlink to
  // .../chrome.LATEST as needed.  This code needs to run only
  // after the migration (i.e. the addition of chrome.LATEST).
  if (symlink_path.Extension() == ".LATEST") {
    base::FilePath extensionless_path = symlink_path.ReplaceExtension("");
    base::FilePath target_path;
    bool extensionless_symlink_exists =
        base::ReadSymbolicLink(extensionless_path, &target_path);

    if (target_path != symlink_path) {
      // No link, or wrong link.  Clean up.  This should happen only once in
      // each log directory after the OS version update, but some of those
      // directories may not be accessed for a long time, so this code needs to
      // stay in forever :/
      if (extensionless_symlink_exists &&
          !base::DeleteFile(extensionless_path)) {
        DPLOG(WARNING) << "Cannot delete " << extensionless_path.value();
      }
      // After cleaning up, create the symlink.
      if (!base::CreateSymbolicLink(symlink_path, extensionless_path)) {
        DPLOG(ERROR) << "Cannot create " << extensionless_path.value();
      }
    }
  }

  // If not starting a new log, then just log through the existing symlink, but
  // if the symlink doesn't exist, create it.
  //
  // If starting a new log, then rename the old symlink as
  // symlink_path.PREVIOUS and make a new symlink to a fresh log file.

  // Check for existence of the symlink.
  base::FilePath target_path;
  bool symlink_exists = base::ReadSymbolicLink(symlink_path, &target_path);

  if (symlink_exists && !new_log)
    return target_path;

  // Remove any extension before time-stamping.
  target_path = GenerateTimestampedName(symlink_path.RemoveExtension(),
                                        base::Time::Now());

  if (symlink_exists) {
    base::FilePath previous_symlink_path =
        symlink_path.ReplaceExtension(".PREVIOUS");
    // Rename symlink to .PREVIOUS.  This nukes an existing symlink just like
    // the rename(2) syscall does.
    if (!base::ReplaceFile(symlink_path, previous_symlink_path, nullptr)) {
      DPLOG(WARNING) << "Cannot rename " << symlink_path.value() << " to "
                     << previous_symlink_path.value();
    }
  }
  // If all went well, the symlink no longer exists.  Recreate it.
  base::FilePath relative_target_path = target_path.BaseName();
  if (!base::CreateSymbolicLink(relative_target_path, symlink_path)) {
    DPLOG(ERROR) << "Unable to create symlink " << symlink_path.value()
                 << " pointing at " << relative_target_path.value();
  }
  return target_path;
}

void RemoveSymlinkAndLog(const base::FilePath& link_path,
                         const base::FilePath& target_path) {
  if (::unlink(link_path.value().c_str()) == -1)
    DPLOG(WARNING) << "Unable to unlink symlink " << link_path.value();
  if (target_path != link_path && ::unlink(target_path.value().c_str()) == -1)
    DPLOG(WARNING) << "Unable to unlink log file " << target_path.value();
}

base::FilePath GetSessionLogDir(const base::CommandLine& command_line) {
  std::string log_dir;
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!env->GetVar(env_vars::kSessionLogDir, &log_dir))
    NOTREACHED_IN_MIGRATION();
  return base::FilePath(log_dir);
}

base::FilePath GetSessionLogFile(const base::CommandLine& command_line) {
  return GetSessionLogDir(command_line)
      .Append(GetLogFileName(command_line).BaseName());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
base::FilePath SetUpLogFile(const base::FilePath& target_path, bool new_log) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const bool supports_symlinks =
      !(target_path.IsAbsolute() &&
        base::StartsWith(target_path.value(), kChronosHomeDir));

  // TODO(crbug.com/40225776): Remove the old symlink logic.
  if (supports_symlinks) {
    // As for now, we keep the original log rotation logic on the file system
    // which supports symlinks.
    return SetUpSymlinkIfNeeded(target_path, new_log);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Chrome OS doesn't support symlinks on this file system, so that it uses
  // the rotation logic which doesn't use symlinks.
  if (!new_log) {
    // Keep using the same log file without doing anything.
    return target_path;
  }

  // For backward compatibility, ignore a ".LATEST" extension the way
  // |SetUpSymlinkIfNeeded()| does.
  base::FilePath bare_path = target_path;
  if (target_path.Extension() == ".LATEST") {
    bare_path = target_path.ReplaceExtension("");
  }

  // Try to rotate the log.
  if (!RotateLogFile(bare_path)) {
    PLOG(ERROR) << "Failed to rotate the log file: " << bare_path.value()
                << ". Keeping using the same log file without rotating.";
  }

  return bare_path;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void InitChromeLogging(const base::CommandLine& command_line,
                       OldFileDeletionState delete_old_log_file) {
  DCHECK(!chrome_logging_initialized_)
      << "Attempted to initialize logging when it was already initialized.";
  bool filename_is_handle = false;
  LoggingDestination logging_dest =
      LoggingDestFromCommandLine(command_line, filename_is_handle);
  LogLockingState log_locking_state = LOCK_LOG_FILE;
  base::FilePath log_path;
#if BUILDFLAG(IS_CHROMEOS_ASH)
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
      // For BWSI (Incognito) logins, we want to put the logs in the user
      // profile directory that is created for the temporary session instead
      // of in the system log directory, for privacy reasons.
      if (command_line.HasSwitch(ash::switches::kGuestSession)) {
        log_path = GetSessionLogFile(command_line);
      }

      // Prepares a log file.  We rotate the previous log file and prepare a new
      // log file if we've been asked to delete the old log, since that
      // indicates the start of a new session.
      target_path =
          SetUpLogFile(log_path, delete_old_log_file == DELETE_OLD_LOG_FILE);

      // Because ChromeOS manages the move to a new session by redirecting
      // the link, it shouldn't remove the old file in the logging code,
      // since that will remove the newly created link instead.
      delete_old_log_file = APPEND_TO_OLD_LOG_FILE;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!success) {
    DPLOG(ERROR) << "Unable to initialize logging to " << log_path.value()
                 << " (which should be a link to " << target_path.value()
                 << ")";
    RemoveSymlinkAndLog(log_path, target_path);
    chrome_logging_failed_ = true;
    return;
  }
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  if (!success) {
    DPLOG(ERROR) << "Unable to initialize logging to " << log_path.value();
    chrome_logging_failed_ = true;
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
    std::string env_filename;
    base::Environment::Create()->GetVar(env_vars::kLogFileName, &env_filename);
#if BUILDFLAG(IS_WIN)
    filename = base::UTF8ToWide(env_filename);
#else
    filename = env_filename;
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

#if BUILDFLAG(IS_CHROMEOS)
base::FilePath GenerateTimestampedName(const base::FilePath& base_path,
                                       base::Time timestamp) {
  return base_path.InsertBeforeExtensionASCII(
      base::UnlocalizedTimeFormatWithPattern(timestamp, "_yyMMdd-HHmmss",
                                             icu::TimeZone::getGMT()));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace logging
