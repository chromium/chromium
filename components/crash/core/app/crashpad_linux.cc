// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/crashpad.h"

#include <pthread.h>
#include <sys/prctl.h>

#include <limits>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "components/crash/core/app/crash_switches.h"
#include "content/public/common/content_descriptors.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "base/at_exit.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "third_party/cros_system_api/constants/crash_reporter.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/build_time.h"
#endif

namespace crash_reporter {

namespace {

// TODO(jperaza): This is the first chance handler type used by Breakpad and v8.
// The Crashpad FirstChanceHandler type explicitly declares the third parameter
// to be a ucontext_t* instead of a void*. Using a reinterpret cast to convert
// one function type to another causes calling the handler to fail with SIGILL
// when CFI is enabled. Use a helper with Crashpad's FirstChanceHandler type
// to delegate to the real first chance handler instead of re-interpreting the
// handler itself. This can be removed if the two function types converge.
bool (*g_first_chance_handler)(int, siginfo_t*, void*);

bool FirstChanceHandlerHelper(int signo,
                              siginfo_t* siginfo,
                              ucontext_t* context) {
  return g_first_chance_handler(signo, siginfo, context);
}

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
// Returns /run/crash_reporter/crashpad_ready/<pid>, the file we touch to
// tell crash_reporter that crashpad is ready and it doesn't need to use
// early-crash mode.
base::FilePath GetCrashpadReadyFilename() {
  pid_t pid = getpid();
  base::FilePath path(crash_reporter::kCrashpadReadyDirectory);
  return path.Append(base::NumberToString(pid));
}

// Inform crash_reporter than crashpad is ready to handle crashes by
// touching /run/crash_reporter/crashpad_ready/<pid>.
//
// Before this point, crash_reporter will attempt to handle Chrome crashes
// sent to it by the kernel core_pattern. This gives us a chance to catch
// crashes that happen before crashpad initializes. See code around
// UserCollector::handling_early_chrome_crash_ for details. Once the
// /run/crash_reporter/crashpad_ready/<pid> file exists, however,
// crash_reporter's UserCollector will assume crashpad will deal with the
// crash and can early-return.
//
// We only need this for the browser process; the cores are too large for
// other processes so early crash doesn't attempt to handle them.
void InformCrashReporterThatCrashpadIsReady() {
  base::FilePath path = GetCrashpadReadyFilename();
  // Note: Using a base::File with FLAG_CREATE instead of WriteFile() to
  // avoid symbolic link shenanigans.
  base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(ERROR) << "Could not create " << path << ": "
               << base::File::ErrorToString(file.error_details());
  }

  // Remove file when the program exits. This isn't perfect; if the program
  // crashes, and if the next browser process comes up with the same PID
  // (without an intervening reboot), and that browser process crashes before
  // crashpad initializes, then the crash service won't get the crash report.
  // This is unlikely enough that I think we can risk it, especially since
  // losing a single crash report is not a huge deal.
  base::AtExitManager::RegisterTask(base::BindOnce(&DeleteCrashpadIsReadyFile));
}
#endif  // BUILDFLAG(IS_CHROMEOS_DEVICE)

}  // namespace

void SetFirstChanceExceptionHandler(bool (*handler)(int, siginfo_t*, void*)) {
  DCHECK(!g_first_chance_handler);
  g_first_chance_handler = handler;
  crashpad::CrashpadClient::SetFirstChanceExceptionHandler(
      FirstChanceHandlerHelper);
}

bool GetHandlerSocket(int* fd, pid_t* pid) {
  return crashpad::CrashpadClient::GetHandlerSocket(fd, pid);
}

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
void DeleteCrashpadIsReadyFile() {
  // Attempt delete but do not log errors if the delete fails. The file might
  // not exist if this function is called twice, or if Chrome did a fork-
  // without-exec and this function is not in the same process that called
  // InformCrashReporterThatCrashpadIsReady().
  base::DeleteFile(GetCrashpadReadyFilename());
}
#endif  // BUILDFLAG(IS_CHROMEOS_DEVICE)

namespace internal {

bool PlatformCrashpadInitialization(
    bool initial_client,
    bool browser_process,
    bool embedded_handler,
    const std::string& user_data_dir,
    const base::FilePath& exe_path,
    const std::vector<std::string>& initial_arguments,
    base::FilePath* database_path) {
  DCHECK_EQ(initial_client, browser_process);
  DCHECK(initial_arguments.empty());

  // Not used on Linux.
  DCHECK(!embedded_handler);
  DCHECK(exe_path.empty());

  crashpad::CrashpadClient client;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string crash_loop_before =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kCrashLoopBefore);
  if (!crash_loop_before.empty()) {
    uint64_t crash_loop_before_time;
    if (!base::StringToUint64(crash_loop_before, &crash_loop_before_time)) {
      LOG(ERROR) << "Couldn't parse --crash-loop-before=" << crash_loop_before;
      DCHECK(false);
      crash_loop_before_time = std::numeric_limits<uint64_t>::max();
    }
    client.SetCrashLoopBefore(crash_loop_before_time);
  }
#endif

  CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
  if (initial_client) {
    base::FilePath metrics_path;
    crash_reporter_client->GetCrashDumpLocation(database_path);
    crash_reporter_client->GetCrashMetricsLocation(&metrics_path);

    base::FilePath handler_path;
    if (!base::PathService::Get(base::DIR_EXE, &handler_path)) {
      return false;
    }
    handler_path = handler_path.Append("chrome_crashpad_handler");

    // When --use-cros-crash-reporter is set (below), the handler passes dumps
    // to ChromeOS's /sbin/crash_reporter which in turn passes the dump to
    // crash_sender which handles the upload.
    std::string url;
#if !BUILDFLAG(IS_CHROMEOS_DEVICE)
    url = crash_reporter_client->GetUploadUrl();
#else
    url = std::string();
#endif

    std::string product_name, product_version, channel;
    crash_reporter_client->GetProductNameAndVersion(&product_name,
                                                    &product_version, &channel);

    std::map<std::string, std::string> annotations;
    annotations["prod"] = product_name;
    annotations["ver"] = product_version;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // Empty means stable.
    const bool allow_empty_channel = true;
    if (channel == "extended") {
      // Extended stable reports as stable (empty string) with an extra bool.
      channel.clear();
      annotations["extended_stable_channel"] = "true";
    }
#else
    const bool allow_empty_channel = false;
#endif
    if (allow_empty_channel || !channel.empty()) {
      annotations["channel"] = channel;
    }

    annotations["plat"] = std::string("Linux");

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // "build_time_millis" is used on LaCros chrome to determine when to stop
    // sending crash reports (from outdated versions of the browser).
    int64_t build_time =
        (base::GetBuildTime() - base::Time::UnixEpoch()).InMilliseconds();
    annotations["build_time_millis"] = base::NumberToString(build_time);
#endif

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
    // Chromium OS: save board and builder path for 'tast symbolize'.
    annotations["chromeos-board"] = base::SysInfo::GetLsbReleaseBoard();
    std::string builder_path;
    if (base::SysInfo::GetLsbReleaseValue("CHROMEOS_RELEASE_BUILDER_PATH",
                                          &builder_path)) {
      annotations["chromeos-builder-path"] = builder_path;
    }

#else
    // Other Linux: save lsb-release. This isn't needed on Chromium OS,
    // where crash_reporter provides it's own values for lsb-release.
    annotations["lsb-release"] = base::GetLinuxDistro();
#endif

    std::vector<std::string> arguments;
    if (crash_reporter_client->ShouldMonitorCrashHandlerExpensively()) {
      arguments.push_back("--monitor-self");
    }

    // Set up --monitor-self-annotation even in the absence of --monitor-self
    // so that minidumps produced by Crashpad's generate_dump tool will
    // contain these annotations.
    arguments.push_back("--monitor-self-annotation=ptype=crashpad-handler");

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
    arguments.push_back("--use-cros-crash-reporter");

    if (crash_reporter_client->IsRunningUnattended()) {
      arguments.push_back(base::StringPrintf("--minidump-dir-for-tests=%s",
                                             database_path->value().c_str()));
      arguments.push_back("--always-allow-feedback");
    }
#endif

    CHECK(client.StartHandler(handler_path, *database_path, metrics_path, url,
                              annotations, arguments, false, false));
  } else {
    int fd = base::GlobalDescriptors::GetInstance()->Get(kCrashDumpSignal);

    pid_t pid = 0;
    if (!sandbox::NamespaceSandbox::InNewUserNamespace()) {
      std::string pid_string =
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kCrashpadHandlerPid);
      bool parsed = base::StringToInt(pid_string, &pid);
      DCHECK(parsed);
    }

    // SIGSYS handling is reserved for the sandbox.
    client.SetUnhandledSignals({SIGSYS});

    client.SetHandlerSocket(crashpad::ScopedFileHandle(fd), pid);

    *database_path = base::FilePath();
  }

  // In the not-large-dumps case record enough extra memory to be able to save
  // dereferenced memory from all registers on the crashing thread. crashpad may
  // save 512-bytes per register, and the largest register set (not including
  // stack pointers) is ARM64 with 32 registers. Hence, 16 KiB.
  const uint32_t kIndirectMemoryLimit =
      crash_reporter_client->GetShouldDumpLargerDumps() ? 4 * 1024 * 1024
                                                        : 16 * 1024;
  crashpad::CrashpadInfo::GetCrashpadInfo()
      ->set_gather_indirectly_referenced_memory(crashpad::TriState::kEnabled,
                                                kIndirectMemoryLimit);

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  if (initial_client) {
    InformCrashReporterThatCrashpadIsReady();
  }
#endif

  return true;
}

}  // namespace internal
}  // namespace crash_reporter
