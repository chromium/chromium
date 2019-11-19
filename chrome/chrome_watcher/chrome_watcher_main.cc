// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <sddl.h>

#include <utility>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/environment.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/logging_win.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/win/process_startup_helper.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "chrome/chrome_watcher/chrome_watcher_main_api.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"  // For chrome::DIR_LOGS
#include "chrome/common/env_vars.h"
#include "chrome/common/win/eventlog_messages.h"
#include "chrome/install_static/initialize_from_primary_module.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "components/browser_watcher/endsession_watcher_window_win.h"
#include "components/browser_watcher/exit_code_watcher_win.h"
#include "components/browser_watcher/window_hang_monitor_win.h"
#include "content/public/common/content_switches.h"

namespace logging {

namespace {

ScopedLogAssertHandler* assert_handler_ = nullptr;

// This should be true for exactly the period between the end of
// InitChromeLogging() and the beginning of CleanupChromeLogging().
bool chrome_logging_initialized_ = false;

// Set if we called InitChromeLogging() but failed to initialize.
bool chrome_logging_failed_ = false;

// Assertion handler for logging errors that occur when dialogs are
// silenced.  To record a new error, pass the log string associated
// with that error in the str parameter.
NOINLINE void SilentRuntimeAssertHandler(const char* file,
                                         int line,
                                         const base::StringPiece message,
                                         const base::StringPiece stack_trace) {
  base::debug::BreakDebugger();
}

// Suppresses error/assertion dialogs and enables the logging of
// those errors into silenced_errors_.
void SuppressDialogs() {
  assert_handler_ =
      new ScopedLogAssertHandler(base::Bind(SilentRuntimeAssertHandler));

  UINT new_flags =
      SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX;

  // Preserve existing error mode, as discussed at http://t/dmea
  UINT existing_flags = SetErrorMode(new_flags);
  SetErrorMode(existing_flags | new_flags);
}

}  // namespace

LoggingDestination DetermineLoggingDestination(
    const base::CommandLine& command_line) {
// only use OutputDebugString in debug mode
#ifdef NDEBUG
  bool enable_logging = false;
  const char* kInvertLoggingSwitch = switches::kEnableLogging;
  const LoggingDestination kDefaultLoggingMode = LOG_TO_FILE;
#else
  bool enable_logging = true;
  const char* kInvertLoggingSwitch = switches::kDisableLogging;
  const LoggingDestination kDefaultLoggingMode = LOG_TO_ALL;
#endif

  if (command_line.HasSwitch(kInvertLoggingSwitch))
    enable_logging = !enable_logging;

  LoggingDestination log_mode;
  if (enable_logging) {
    // Let --enable-logging=stderr force only stderr, particularly useful for
    // non-debug builds where otherwise you can't get logs to stderr at all.
    if (command_line.GetSwitchValueASCII(switches::kEnableLogging) == "stderr")
      log_mode = LOG_TO_SYSTEM_DEBUG_LOG | LOG_TO_STDERR;
    else
      log_mode = kDefaultLoggingMode;
  } else {
    log_mode = LOG_NONE;
  }
  return log_mode;
}

bool GetLogsPath(base::FilePath* result) {
#ifdef NDEBUG
  // Release builds write to the data dir. This is a copy of the Windows
  // implementation of GetDefaultUserDataDirectory().
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, result))
    return false;
  *result = result->Append(install_static::GetChromeInstallSubDirectory());
  *result = result->Append(chrome::kUserDataDirname);
  return true;

#else
  // Debug builds write next to the binary (in the build tree)
  return base::PathService::Get(base::DIR_EXE, result);
#endif  // NDEBUG
}

base::FilePath GetLogFileName(const base::CommandLine& command_line) {
  std::string filename = command_line.GetSwitchValueASCII(switches::kLogFile);
  if (filename.empty())
    base::Environment::Create()->GetVar(env_vars::kLogFileName, &filename);
  if (!filename.empty())
    return base::FilePath::FromUTF8Unsafe(filename);

  const base::FilePath log_filename(FILE_PATH_LITERAL("chrome_debug.log"));
  base::FilePath log_path;

  if (GetLogsPath(&log_path)) {
    log_path = log_path.Append(log_filename);
    return log_path;
  } else {
    // error with path service, just use some default file somewhere
    return log_filename;
  }
}

// This function was mostly copied from InitChromeLogging(). Copying it was
// necessary to avoid pulling in a long-tail of unneeded code from
// //chrome/common.
void InitChromeWatcherLogging(const base::CommandLine& command_line,
                              OldFileDeletionState delete_old_log_file) {
  DCHECK(!chrome_logging_initialized_)
      << "Attempted to initialize logging when it was already initialized.";

  LoggingDestination logging_dest = DetermineLoggingDestination(command_line);
  LogLockingState log_locking_state = LOCK_LOG_FILE;
  base::FilePath log_path;

  // Don't resolve the log path unless we need to. Otherwise we leave an open
  // ALPC handle after sandbox lockdown on Windows.
  if ((logging_dest & LOG_TO_FILE) != 0) {
    log_path = GetLogFileName(command_line);

  } else {
    log_locking_state = DONT_LOCK_LOG_FILE;
  }

  LoggingSettings settings;
  settings.logging_dest = logging_dest;
  settings.log_file_path = log_path.value().c_str();
  settings.lock_log = log_locking_state;
  settings.delete_old = delete_old_log_file;
  bool success = InitLogging(settings);

  if (!success) {
    DPLOG(ERROR) << "Unable to initialize logging to " << log_path.value();
    chrome_logging_failed_ = true;
    return;
  }

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
        level < LOG_NUM_SEVERITIES) {
      SetMinLogLevel(level);
    } else {
      DLOG(WARNING) << "Bad log level: " << log_level;
    }
  }

  // Enable logging to the Windows Event Log.
  SetEventSource(base::UTF16ToASCII(
                     install_static::InstallDetails::Get().install_full_name()),
                 BROWSER_CATEGORY, MSG_LOG_MESSAGE);

  base::StatisticsRecorder::InitLogOnShutdown();

  chrome_logging_initialized_ = true;
}
}  // namespace logging

namespace {

// Use the same log facility as Chrome for convenience.
// {7FE69228-633E-4f06-80C1-527FEA23E3A7}
const GUID kChromeWatcherTraceProviderName = {
    0x7fe69228, 0x633e, 0x4f06,
        { 0x80, 0xc1, 0x52, 0x7f, 0xea, 0x23, 0xe3, 0xa7 } };

// The amount of time we wait around for a WM_ENDSESSION or a process exit.
const int kDelayTimeSeconds = 30;

// Takes care of monitoring a browser. This class watches for a browser's exit
// code, as well as listening for WM_ENDSESSION messages. Events are recorded in
// an exit funnel, for reporting the next time Chrome runs.
class BrowserMonitor {
 public:
  BrowserMonitor(base::StringPiece16 registry_path, base::RunLoop* run_loop);
  ~BrowserMonitor();

  // Initiates the asynchronous monitoring process, returns true on success.
  // |on_initialized_event| will be signaled immediately before blocking on the
  // exit of |process|.
  bool StartWatching(base::Process process,
                     base::win::ScopedHandle on_initialized_event);

 private:
  // Called from EndSessionWatcherWindow on a end session messages.
  void OnEndSessionMessage(UINT message, LPARAM lparam);

  // Blocking function that runs on |background_thread_|. Signals
  // |on_initialized_event| before waiting for the browser process to exit.
  void Watch(base::win::ScopedHandle on_initialized_event);

  // Posted to main thread from Watch when browser exits.
  void BrowserExited();

  browser_watcher::ExitCodeWatcher exit_code_watcher_;
  browser_watcher::EndSessionWatcherWindow end_session_watcher_window_;

  // The thread that runs Watch().
  base::Thread background_thread_;

  // Set when the browser has exited, used to stretch the watcher's lifetime
  // when WM_ENDSESSION occurs before browser exit.
  base::WaitableEvent browser_exited_;

  // The run loop for the main thread and its task runner.
  base::RunLoop* run_loop_;
  scoped_refptr<base::SequencedTaskRunner> main_thread_;

  DISALLOW_COPY_AND_ASSIGN(BrowserMonitor);
};

BrowserMonitor::BrowserMonitor(base::StringPiece16 registry_path,
                               base::RunLoop* run_loop)
    : exit_code_watcher_(registry_path),
      end_session_watcher_window_(
          base::Bind(&BrowserMonitor::OnEndSessionMessage,
                     base::Unretained(this))),
      background_thread_("BrowserWatcherThread"),
      browser_exited_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      run_loop_(run_loop),
      main_thread_(base::ThreadTaskRunnerHandle::Get()) {}

BrowserMonitor::~BrowserMonitor() {
}

bool BrowserMonitor::StartWatching(
    base::Process process,
    base::win::ScopedHandle on_initialized_event) {
  if (!exit_code_watcher_.Initialize(std::move(process)))
    return false;

  if (!background_thread_.StartWithOptions(
          base::Thread::Options(base::MessagePumpType::IO, 0))) {
    return false;
  }

  if (!background_thread_.task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&BrowserMonitor::Watch, base::Unretained(this),
                         std::move(on_initialized_event)))) {
    background_thread_.Stop();
    return false;
  }

  return true;
}

void BrowserMonitor::OnEndSessionMessage(UINT message, LPARAM lparam) {
  DCHECK_EQ(main_thread_, base::ThreadTaskRunnerHandle::Get());

  // If the browser hasn't exited yet, dally for a bit to try and stretch this
  // process' lifetime to give it some more time to capture the browser exit.
  browser_exited_.TimedWait(base::TimeDelta::FromSeconds(kDelayTimeSeconds));

  run_loop_->Quit();
}

void BrowserMonitor::Watch(base::win::ScopedHandle on_initialized_event) {
  // This needs to run on an IO thread.
  DCHECK_NE(main_thread_, base::ThreadTaskRunnerHandle::Get());

  // Signal our client that we have cleared all of the obstacles that might lead
  // to an early exit.
  ::SetEvent(on_initialized_event.Get());
  on_initialized_event.Close();

  exit_code_watcher_.WaitForExit();

  // Note that the browser has exited.
  browser_exited_.Signal();

  main_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserMonitor::BrowserExited, base::Unretained(this)));
}

void BrowserMonitor::BrowserExited() {
  // This runs in the main thread.
  DCHECK_EQ(main_thread_, base::ThreadTaskRunnerHandle::Get());

  // Our background thread has served it's purpose.
  background_thread_.Stop();

  const int exit_code = exit_code_watcher_.exit_code();
  if (exit_code >= 0 && exit_code <= 28) {
    // The browser exited with a well-known exit code, quit this process
    // immediately.
    run_loop_->Quit();
  } else {
    // The browser exited abnormally, wait around for a little bit to see
    // whether this instance will get a logoff message.
    main_thread_->PostDelayedTask(
        FROM_HERE,
        run_loop_->QuitClosure(),
        base::TimeDelta::FromSeconds(kDelayTimeSeconds));
  }
}

}  // namespace

// The main entry point to the watcher, declared as extern "C" to avoid name
// mangling.
extern "C" int WatcherMain(const base::char16* registry_path,
                           HANDLE process_handle,
                           DWORD main_thread_id,
                           HANDLE on_initialized_event_handle,
                           const base::char16* browser_data_directory) {
  install_static::InitializeFromPrimaryModule();
  base::Process process(process_handle);
  base::win::ScopedHandle on_initialized_event(on_initialized_event_handle);

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  // Initialize the commandline singleton from the environment.
  base::CommandLine::Init(0, nullptr);
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();

  logging::InitChromeWatcherLogging(cmd_line, logging::APPEND_TO_OLD_LOG_FILE);
  logging::LogEventProvider::Initialize(kChromeWatcherTraceProviderName);

  // Arrange to be shut down as late as possible, as we want to outlive
  // chrome.exe in order to report its exit status.
  ::SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY);

  // Make sure the process exits cleanly on unexpected errors.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(cmd_line);

  // Run a UI task executor on the main thread.
  base::PlatformThread::SetName("WatcherMainThread");
  base::SingleThreadTaskExecutor main_thread_task_executor(
      base::MessagePumpType::UI);

  base::RunLoop run_loop;
  BrowserMonitor monitor(registry_path, &run_loop);
  if (!monitor.StartWatching(process.Duplicate(),
                             std::move(on_initialized_event))) {
    return 1;
  }
  run_loop.Run();
  // TODO(manzagop): hang monitoring using WindowHangMonitor.

  // Wind logging down.
  logging::LogEventProvider::Uninitialize();

  return 0;
}

static_assert(
    std::is_same<decltype(&WatcherMain), ChromeWatcherMainFunction>::value,
    "WatcherMain() has wrong type");
