// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater.h"

#include <algorithm>
#include <iterator>
#include <optional>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/process/memory.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/app_install.h"
#include "chrome/updater/app/app_net_worker.h"
#include "chrome/updater/app/app_patch_worker.h"
#include "chrome/updater/app/app_recover.h"
#include "chrome/updater/app/app_server.h"
#include "chrome/updater/app/app_uninstall.h"
#include "chrome/updater/app/app_uninstall_self.h"
#include "chrome/updater/app/app_unzip_worker.h"
#include "chrome/updater/app/app_update.h"
#include "chrome/updater/app/app_update_apps.h"
#include "chrome/updater/app/app_wake.h"
#include "chrome/updater/app/app_wakeall.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#include "chrome/updater/event_history.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/usage_stats_permissions.h"
#include "chrome/updater/util/util.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

#if BUILDFLAG(IS_WIN)
#include <sysinfoapi.h>

#include "base/cpu.h"
#include "base/debug/alias.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/to_string.h"
#include "base/win/process_startup_helper.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "chrome/updater/app/server/win/updater_service_delegate.h"
#include "chrome/updater/util/win_util.h"
#include "partition_alloc/page_allocator.h"
#elif BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif

// Instructions For Windows.
// - To install only the updater, run "updatersetup.exe" from the build out dir.
// - To install Chrome and the updater, do the same but use the --app-id:
//    updatersetup.exe --install --app-id={8A69D345-D564-463c-AFF1-A69D9E530F96}
// - To uninstall, run "updater.exe --uninstall" from its install directory,
// which is under %LOCALAPPDATA%\Google\GoogleUpdater, or from the |out|
// directory of the build.
// - To debug, append the following arguments to any updater command line:
//    --enable-logging --vmodule=*/chrome/updater/*=2,*/components/winhttp/*=2.

namespace updater {
namespace {

void ReinitializeLoggingAfterCrashHandler(UpdaterScope updater_scope) {
  // Initializing the logging more than two times is not supported. In this
  // case, logging has been initialized once in the updater main, and the
  // the second time by the crash handler.
  // Reinitializing the log is not possible if the vlog switch is
  // already present on the command line. The code in this function relies
  // on undocumented behavior of the logging object, and it could break.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(kLoggingModuleSwitch);
  InitLogging(updater_scope);
}

void InitializeCrashReporting(UpdaterScope updater_scope) {
  if (!CrashClient::GetInstance()->InitializeCrashReporting(updater_scope)) {
    VLOG(1) << "Crash reporting is not available.";
    return;
  }
  crash_reporter::InitializeCrashKeys();
  crash_keys::SetSwitchesFromCommandLine(
      *base::CommandLine::ForCurrentProcess(), nullptr);
  VLOG(1) << "Crash reporting initialized.";
}

int HandleUpdaterCommands(UpdaterScope updater_scope,
                          const base::CommandLine* command_line) {
  // Used for unit test purposes. There is no need to run with a crash handler.
  if (command_line->HasSwitch(kTestSwitch)) {
    return kErrorOk;
  }

  if (command_line->HasSwitch(kCrashHandlerSwitch)) {
    const int retval = CrashReporterMain();

    // The crash handler mutates the logging object, so the updater process
    // stops logging to the log file aftern `CrashReporterMain()` returns.
    ReinitializeLoggingAfterCrashHandler(updater_scope);
    return retval;
  }

  // Starts and connects to the external crash handler as early as possible.
  StartCrashReporter(updater_scope, kUpdaterVersion);

  InitializeCrashReporting(updater_scope);

  InitializeThreadPool("updater");
  const base::ScopedClosureRunner shutdown_thread_pool(base::BindOnce([] {
    // For the updater, it is important to join all threads before `UpdaterMain`
    // exits, otherwise the behavior of the program is undefined. The threads
    // in the pool can still run after shutdown to handle CONTINUE_ON_SHUTDOWN
    // tasks, for example. In Chrome, the thread pool is leaked for this reason
    // and there is no way to join its threads in production code. The updater
    // has no such requirements (crbug.com/1484776).
    base::ThreadPoolInstance* thread_pool = base::ThreadPoolInstance::Get();
    thread_pool->Shutdown();
    thread_pool->JoinForTesting();  // IN-TEST
    base::ThreadPoolInstance::Set(nullptr);
  }));

#if BUILDFLAG(IS_WIN)
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  if (!com_initializer.Succeeded()) {
    PLOG(ERROR) << "Failed to initialize COM";
    return kErrorComInitializationFailed;
  }

  // Failing to disable COM exception handling is a critical error.
  CHECK(SUCCEEDED(DisableCOMExceptionHandling()))
      << "Failed to disable COM exception handling.";
  base::win::RegisterInvalidParamHandler();
  VLOG(1) << GetUACState();
#elif BUILDFLAG(IS_MAC)
  base::apple::SetBaseBundleIDOverride(MAC_BUNDLE_IDENTIFIER_STRING);
#endif

  // Records a backtrace in the log, crashes the program, saves a crash dump,
  // and reports the crash.
  CHECK(!command_line->HasSwitch(kCrashMeSwitch)) << "--crash-me was used.";

  // As long as this object is alive, all Mojo API surface relevant to IPC
  // connections is usable, and message pipes which span a process boundary will
  // continue to function.
  ScopedIPCSupportWrapper ipc_support;

  // Only tasks and timers are supported on the main sequence.
  base::SingleThreadTaskExecutor main_task_executor(
      base::MessagePumpType::DEFAULT, true);

  if (command_line->HasSwitch(kForceInstallSwitch)) {
    const int recover_result = MakeAppRecover()->Run();
    return recover_result == kErrorOk &&
                   (command_line->HasSwitch(kInstallSwitch) ||
                    command_line->HasSwitch(kHandoffSwitch))
               ? MakeAppInstall(command_line->HasSwitch(kSilentSwitch))->Run()
               : recover_result;
  }

  if (command_line->HasSwitch(kInstallSwitch) ||
      command_line->HasSwitch(kHandoffSwitch)) {
    return MakeAppInstall(command_line->HasSwitch(kSilentSwitch))->Run();
  }

  if (command_line->HasSwitch(kServerSwitch)) {
    return MakeAppServer()->Run();
  }

  if (command_line->HasSwitch(kUpdateSwitch)) {
    return MakeAppUpdate()->Run();
  }

  if (command_line->HasSwitch(kUpdateAppsSwitch)) {
    return MakeAppUpdateApps()->Run();
  }

#if BUILDFLAG(IS_WIN)
  if (command_line->HasSwitch(kWindowsServiceSwitch)) {
    return UpdaterServiceDelegate::RunWindowsService();
  }

  if (command_line->HasSwitch(kHealthCheckSwitch)) {
    return kErrorOk;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (command_line->HasSwitch(kUninstallSwitch) ||
      command_line->HasSwitch(kUninstallIfUnusedSwitch)) {
    return MakeAppUninstall()->Run();
  }

  if (command_line->HasSwitch(kUninstallSelfSwitch)) {
    return MakeAppUninstallSelf()->Run();
  }

  if (command_line->HasSwitch(kRecoverSwitch) ||
      command_line->HasSwitch(kBrowserVersionSwitch)) {
    return MakeAppRecover()->Run();
  }

  if (command_line->HasSwitch(kWakeSwitch)) {
    return MakeAppWake()->Run();
  }

  if (command_line->HasSwitch(kWakeAllSwitch)) {
    return MakeAppWakeAll()->Run();
  }

  if (command_line->HasSwitch(kPatchWorkerSwitch)) {
    return MakeAppPatchWorker()->Run();
  }

  if (command_line->HasSwitch(kUnzipWorkerSwitch)) {
    return MakeAppUnzipWorker()->Run();
  }

#if BUILDFLAG(IS_MAC)
  if (command_line->HasSwitch(kNetWorkerSwitch)) {
    return MakeAppNetWorker()->Run();
  }
#endif  // BUILDFLAG(IS_MAC)

  VLOG(1) << "Unknown command line switch.";
  return kErrorUnknownCommandLine;
}

// Returns the string literal corresponding to an updater command, which
// is present on the updater process command line. Returns an empty string
// if the command is not found.
const char* GetUpdaterCommand(const base::CommandLine* command_line) {
  // Contains the literals which are associated with specific updater commands.
  static constexpr const char* commands[] = {
      kWindowsServiceSwitch,
      kCrashHandlerSwitch,
      kInstallSwitch,
      kRecoverSwitch,
      kServerSwitch,
      kTestSwitch,
      kUninstallIfUnusedSwitch,
      kUninstallSelfSwitch,
      kUninstallSwitch,
      kUpdateSwitch,
      kWakeSwitch,
      kWakeAllSwitch,
      kHealthCheckSwitch,
      kHandoffSwitch,
      kNetWorkerSwitch,
      kUnzipWorkerSwitch,
      kPatchWorkerSwitch,
  };
  const auto it = std::ranges::find_if(commands, [command_line](auto cmd) {
    return command_line->HasSwitch(cmd);
  });
  // Return the command. As a workaround for recovery component invocations
  // that do not pass --recover, report the browser version switch as --recover.
  return it != std::end(commands)
             ? *it
             : (command_line->HasSwitch(kBrowserVersionSwitch) ? kRecoverSwitch
                                                               : "");
}

constexpr const char* BuildFlavor() {
#if defined(NDEBUG)
  return "opt";
#else
  return "debug";
#endif
}

std::string OperatingSystemVersion() {
#if BUILDFLAG(IS_WIN)
  const base::win::OSInfo::VersionNumber v =
      base::win::OSInfo::GetInstance()->version_number();
  return base::StringPrintf("%u.%u.%u.%u", v.major, v.minor, v.build, v.patch);
#else
  return base::SysInfo().OperatingSystemVersion();
#endif
}

base::CommandLine::StringType GetCommandLineString() {
#if BUILDFLAG(IS_WIN)
  // Gets the raw command line on Windows, because
  // `base::CommandLine::GetCommandLineString()` could return an invalid string
  // after the class re-arranges the legacy command line arguments.
  return ::GetCommandLine();
#else
  return base::CommandLine::ForCurrentProcess()->GetCommandLineString();
#endif
}

void EnableLoggingByDefault() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kEnableLoggingSwitch)) {
    command_line->AppendSwitch(kEnableLoggingSwitch);
  }
  if (!command_line->HasSwitch(kLoggingModuleSwitch)) {
    command_line->AppendSwitchUTF8(kLoggingModuleSwitch,
                                   kLoggingModuleSwitchValue);
  }
}

#if BUILDFLAG(IS_WIN)
std::string MemoryStatus() {
  MEMORYSTATUSEX memory_status = {};
  memory_status.dwLength = sizeof(memory_status);
  return ::GlobalMemoryStatusEx(&memory_status)
             ? base::StringPrintf("available: %dK, total: %dK",
                                  memory_status.ullAvailPageFile / 1024,
                                  memory_status.ullTotalPageFile / 1024)
             : std::string("n/a");
}

// Assumes that 10MB of available memory is needed for the process to run.
// Windows may extend its page file when the total memory commitment gets
// close to the commit limit. Tries a large allocation, and keeps looping
// if the memory allocator returns an error indicating that the page file
// is too small.
void EnsureEnoughMemory() {
  VLOG(1) << MemoryStatus();

  MEMORYSTATUSEX memory_status = {};
  memory_status.dwLength = sizeof(memory_status);
  if (!::GlobalMemoryStatusEx(&memory_status)) {
    VLOG(1) << "Can't memory stat: " << std::hex << ::GetLastError();
    return;
  }
  constexpr SIZE_T kMinMemoryNeeded = 10'000'000;  // 10MB.
  if (memory_status.ullAvailPageFile >= kMinMemoryNeeded) {
    return;
  }
  if (void* alloc = []() -> void* {
        constexpr int kMaxTries = 25;
        constexpr int kDelayMs = 50;
        for (int tries = 0; tries < kMaxTries; ++tries) {
          void* ret = ::VirtualAlloc(NULL, kMinMemoryNeeded,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
          if (ret || [] {
                switch (::GetLastError()) {
                  case ERROR_COMMITMENT_MINIMUM:
                  case ERROR_COMMITMENT_LIMIT:
                  case ERROR_NOT_ENOUGH_MEMORY:
                  case ERROR_PAGEFILE_QUOTA:
                    return false;  // Retry on page file related errors.
                  default:
                    return true;  // Don't retry.
                }
              }()) {
            return ret;
          }
          ::Sleep(kDelayMs);
        }
        return nullptr;
      }();
      alloc) {
    ::VirtualFree(alloc, 0, MEM_RELEASE);
  } else {
    VLOG(1) << "Allocation failed: " << kMinMemoryNeeded / 1024 << "K, "
            << std::hex << ::GetLastError();
  }

  VLOG(1) << MemoryStatus();
}

void RecordCpuFeaturesForCrash() {
#if defined(ARCH_CPU_X86_FAMILY)
  base::CPU cpu;
  static crash_reporter::CrashKeyString<6> crash_key_aesni("aesni");
  crash_key_aesni.Set(base::ToString(cpu.has_aesni()));
  static crash_reporter::CrashKeyString<6> crash_key_avx512f("avx512f");
  crash_key_avx512f.Set(base::ToString(cpu.has_avx512_f()));
  static crash_reporter::CrashKeyString<6> crash_key_in_vm("invm");
  crash_key_in_vm.Set(base::ToString(cpu.is_running_in_vm()));
#endif
}

#endif  // IS_WIN

}  // namespace

int UpdaterMain(int argc, const char* const* argv) {
#if BUILDFLAG(IS_WIN)
  CHECK(EnableSecureDllLoading());
#endif

  // Make the process more resilient to memory allocation issues.
#if BUILDFLAG(IS_WIN)
  EnableProcessHeapMetadataProtection();
  partition_alloc::SetRetryOnCommitFailure(true);
#endif
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  logging::RegisterAbslAbortHook();

  base::PlatformThread::SetName("UpdaterMain");
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_WIN)
  *command_line = GetCommandLineLegacyCompatible();
#endif
  EnableLoggingByDefault();
  const UpdaterScope updater_scope = GetUpdaterScope();
  InitLogging(updater_scope);
  InitHistoryLogging(updater_scope);
  const base::ProcessId parent_pid =
      base::GetParentProcessId(base::GetCurrentProcessHandle());
  VLOG(1) << "Version: " << kUpdaterVersion << ", " << BuildFlavor() << ", "
          << base::SysInfo::ProcessCPUArchitecture()
          << ", command line: " << GetCommandLineString();
  VLOG(1) << "OS version: " << OperatingSystemVersion()
          << ", System uptime (seconds): "
          << base::SysInfo::Uptime().InSeconds()
          << ", parent pid: " << parent_pid;

#if BUILDFLAG(IS_WIN)
  const HResultOr<std::wstring> cmd_line = GetCommandLineForPid(parent_pid);
  if (cmd_line.has_value()) {
    VLOG(1) << "Parent process command line: " << *cmd_line;
  }
  EnsureEnoughMemory();
  RecordCpuFeaturesForCrash();  // TODO(crbug.com/441591130): remove when fixed.
#endif                          // IS_WIN

  const std::string event_id = GenerateEventId();
#if BUILDFLAG(IS_WIN)
  const std::string command_line_string =
      base::SysWideToUTF8(GetCommandLineString());
#else
  const std::string command_line_string = GetCommandLineString();
#endif
  UpdaterProcessStartEvent()
      .SetEventId(event_id)
      .SetCommandLine(command_line_string)
      .SetTimestamp(base::Time::Now())
      .SetUpdaterVersion(kUpdaterVersion)
      .SetScope(updater_scope)
      .SetOsPlatform(base::SysInfo::OperatingSystemName())
      .SetOsArchitecture(base::SysInfo::OperatingSystemArchitecture())
      .SetUpdaterArchitecture(base::SysInfo::ProcessCPUArchitecture())
      .SetParentPid(parent_pid)
      .Write();
  const int exit_code = HandleUpdaterCommands(updater_scope, command_line);
  UpdaterProcessEndEvent().SetEventId(event_id).SetExitCode(exit_code).Write();
  VLOG(1) << __func__ << " (--" << GetUpdaterCommand(command_line) << ")"
          << " returned " << exit_code << ".";

#if BUILDFLAG(IS_WIN)
  base::AtExitManager::ProcessCallbacksNow();

  ::SetLastError(ERROR_SUCCESS);
  const bool terminate_result =
      ::TerminateProcess(::GetCurrentProcess(), static_cast<UINT>(exit_code));

  // Capture error information in case TerminateProcess fails so that it may be
  // found in a post-return crash dump if the process crashes on exit.
  const DWORD terminate_error_code = ::GetLastError();
  DWORD exit_codes[] = {
      0xDEADBECF,
      static_cast<DWORD>(exit_code),
      static_cast<DWORD>(terminate_result),
      terminate_error_code,
      0xDEADBEDF,
  };
  base::debug::Alias(exit_codes);
#endif  // BUILDFLAG(IS_WIN)

  return exit_code;
}

}  // namespace updater
