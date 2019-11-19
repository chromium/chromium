// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/crashpad.h"

#include <pthread.h>
#include <sys/prctl.h>

#include <limits>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "base/strings/string_number_conversions.h"
#include "build/branding_buildflags.h"
#include "components/crash/content/app/crash_reporter_client.h"
#include "components/crash/content/app/crash_switches.h"
#include "content/public/common/content_descriptors.h"
#include "sandbox/linux/services/namespace_sandbox.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"

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

}  // namespace

void SetFirstChanceExceptionHandler(bool (*handler)(int, siginfo_t*, void*)) {
  DCHECK(!g_first_chance_handler);
  g_first_chance_handler = handler;
  crashpad::CrashpadClient::SetFirstChanceExceptionHandler(
      FirstChanceHandlerHelper);
}

// TODO(jperaza): Remove kEnableCrashpad and IsCrashpadEnabled() when Crashpad
// is fully enabled on Linux.
const char kEnableCrashpad[] = "enable-crashpad";

bool IsCrashpadEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kEnableCrashpad);
}

bool GetHandlerSocket(int* fd, pid_t* pid) {
  return crashpad::CrashpadClient::GetHandlerSocket(fd, pid);
}

void SetPtracerAtFork() {
  pid_t pid;
  if (!GetHandlerSocket(nullptr, &pid)) {
    return;
  }
  if (pid > 0 && prctl(PR_SET_PTRACER, pid, 0, 0, 0) != 0) {
    PLOG(ERROR) << "prctl";
  }
}

namespace internal {

base::FilePath PlatformCrashpadInitialization(
    bool initial_client,
    bool browser_process,
    bool embedded_handler,
    const std::string& user_data_dir,
    const base::FilePath& exe_path,
    const std::vector<std::string>& initial_arguments) {
  DCHECK_EQ(initial_client, browser_process);
  DCHECK(initial_arguments.empty());

  // Not used on Linux.
  DCHECK(!embedded_handler);
  DCHECK(exe_path.empty());

  crashpad::CrashpadClient client;
#if defined(OS_CHROMEOS)
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

  if (initial_client) {
    CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
    base::FilePath database_path, metrics_path;
    crash_reporter_client->GetCrashDumpLocation(&database_path);
    crash_reporter_client->GetCrashMetricsLocation(&metrics_path);

    base::FilePath handler_path;
    if (!base::PathService::Get(base::DIR_EXE, &handler_path)) {
      return database_path;
    }
    handler_path = handler_path.Append("crashpad_handler");

    // When --use-cros-crash-reporter is set (below), the handler passes dumps
    // to ChromeOS's /sbin/crash_reporter which in turn passes the dump to
    // crash_sender which handles the upload.
    std::string url;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OFFICIAL_BUILD) && \
    !defined(OS_CHROMEOS)
    url = "https://clients2.google.com/cr/report";
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
#else
    const bool allow_empty_channel = false;
#endif
    if (allow_empty_channel || !channel.empty()) {
      annotations["channel"] = channel;
    }

    annotations["plat"] = std::string("Linux");

    std::vector<std::string> arguments;
    if (crash_reporter_client->ShouldMonitorCrashHandlerExpensively()) {
      arguments.push_back("--monitor-self");
    }

    // Set up --monitor-self-annotation even in the absence of --monitor-self
    // so that minidumps produced by Crashpad's generate_dump tool will
    // contain these annotations.
    arguments.push_back("--monitor-self-annotation=ptype=crashpad-handler");

#if defined(OS_CHROMEOS)
    arguments.push_back("--use-cros-crash-reporter");
#endif

    bool result =
        client.StartHandler(handler_path, database_path, metrics_path, url,
                            annotations, arguments, false, false);
    DCHECK(result);

    pthread_atfork(nullptr, nullptr, SetPtracerAtFork);
    return database_path;
  }

  int fd = base::GlobalDescriptors::GetInstance()->Get(
      service_manager::kCrashDumpSignal);

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

  pthread_atfork(nullptr, nullptr, SetPtracerAtFork);
  return base::FilePath();
}

}  // namespace internal
}  // namespace crash_reporter
