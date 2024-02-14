// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/crashpad.h"

#include <memory>
#include <string>

#include "base/debug/crash_logging.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/app_launch_prefetch/app_launch_prefetch.h"
#include "components/crash/core/app/crash_export_thunks.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "components/crash/core/app/crash_switches.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/simulate_crash_win.h"

namespace crash_reporter {
namespace internal {

void GetPlatformCrashpadAnnotations(
    std::map<std::string, std::string>* annotations) {
  CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
  wchar_t exe_file[MAX_PATH] = {};
  CHECK(::GetModuleFileName(nullptr, exe_file, std::size(exe_file)));
  std::wstring product_name, version, special_build, channel_name;
  crash_reporter_client->GetProductNameAndVersion(
      exe_file, &product_name, &version, &special_build, &channel_name);
  (*annotations)["prod"] = base::WideToUTF8(product_name);
  (*annotations)["ver"] = base::WideToUTF8(version);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Empty means stable.
  const bool allow_empty_channel = true;
  if (channel_name == L"extended") {
    // Extended stable reports as stable (empty string) with an extra bool.
    channel_name.clear();
    (*annotations)["extended_stable_channel"] = "true";
  }
#else
  const bool allow_empty_channel = false;
#endif
  if (allow_empty_channel || !channel_name.empty())
    (*annotations)["channel"] = base::WideToUTF8(channel_name);
  if (!special_build.empty())
    (*annotations)["special"] = base::WideToUTF8(special_build);
#if defined(ARCH_CPU_X86)
  (*annotations)["plat"] = std::string("Win32");
#elif defined(ARCH_CPU_X86_64)
  (*annotations)["plat"] = std::string("Win64");
#endif
}

bool PlatformCrashpadInitialization(
    bool initial_client,
    bool browser_process,
    bool embedded_handler,
    const std::string& user_data_dir,
    const base::FilePath& exe_path,
    const std::vector<std::string>& initial_arguments,
    base::FilePath* database_path) {
  base::FilePath metrics_path;  // Only valid in the browser process.

  const char kPipeNameVar[] = "CHROME_CRASHPAD_PIPE_NAME";
  const char kServerUrlVar[] = "CHROME_CRASHPAD_SERVER_URL";
  std::unique_ptr<base::Environment> env(base::Environment::Create());

  CrashReporterClient* crash_reporter_client = GetCrashReporterClient();

  bool initialized = false;

  if (initial_client) {
    std::wstring database_path_str;
    if (crash_reporter_client->GetCrashDumpLocation(&database_path_str))
      *database_path = base::FilePath(database_path_str);

    std::wstring metrics_path_str;
    if (crash_reporter_client->GetCrashMetricsLocation(&metrics_path_str)) {
      metrics_path = base::FilePath(metrics_path_str);
      CHECK(base::CreateDirectoryAndGetError(metrics_path, nullptr));
    }

    std::map<std::string, std::string> process_annotations;
    GetPlatformCrashpadAnnotations(&process_annotations);

    std::string url = crash_reporter_client->GetUploadUrl();

    // Allow the crash server to be overridden for testing. If the variable
    // isn't present in the environment then the default URL will remain.
    env->GetVar(kServerUrlVar, &url);

    base::FilePath exe_file(exe_path);
    if (exe_file.empty()) {
      wchar_t exe_file_path[MAX_PATH] = {};
      CHECK(::GetModuleFileName(nullptr, exe_file_path,
                                std::size(exe_file_path)));

      exe_file = base::FilePath(exe_file_path);
    }

    // If the handler is embedded in the binary (e.g. chrome, setup), we
    // reinvoke it with --type=crashpad-handler. Otherwise, we use the
    // standalone crashpad_handler.exe (for tests, etc.).
    std::vector<std::string> start_arguments(initial_arguments);
    if (embedded_handler) {
      start_arguments.push_back(std::string("--type=") +
                                switches::kCrashpadHandler);
      if (!user_data_dir.empty()) {
        start_arguments.push_back(std::string("--user-data-dir=") +
                                  user_data_dir);
      }
      start_arguments.push_back(
          base::WideToUTF8(app_launch_prefetch::GetPrefetchSwitch(
              app_launch_prefetch::SubprocessType::kCrashpad)));
    } else {
      base::FilePath exe_dir = exe_file.DirName();
      exe_file = exe_dir.Append(FILE_PATH_LITERAL("crashpad_handler.exe"));
    }

    std::vector<std::string> arguments(start_arguments);

    if (crash_reporter_client->ShouldMonitorCrashHandlerExpensively()) {
      arguments.push_back("--monitor-self");
      for (const std::string& start_argument : start_arguments) {
        arguments.push_back(std::string("--monitor-self-argument=") +
                            start_argument);
      }
    }

    // Set up --monitor-self-annotation even in the absence of --monitor-self so
    // that minidumps produced by Crashpad's generate_dump tool will contain
    // these annotations.
    arguments.push_back(std::string("--monitor-self-annotation=ptype=") +
                        switches::kCrashpadHandler);

    initialized = GetCrashpadClient().StartHandler(
        exe_file, *database_path, metrics_path, url, process_annotations,
        arguments, /*restartable=*/false, /*asynchronous_start=*/false);

    if (initialized) {
      // If we're the browser, push the pipe name into the environment so child
      // processes can connect to it. If we inherited another crashpad_handler's
      // pipe name, we'll overwrite it here.
      env->SetVar(kPipeNameVar,
                  base::WideToUTF8(GetCrashpadClient().GetHandlerIPCPipe()));
    }
  } else {
    std::string pipe_name_utf8;
    if (env->GetVar(kPipeNameVar, &pipe_name_utf8)) {
      initialized = GetCrashpadClient().SetHandlerIPCPipe(
          base::UTF8ToWide(pipe_name_utf8));
    }
  }

  if (!initialized)
    return false;

  // Regester WER helper only if it will produce useful information - prior to
  // 20H1 the crashes it can help with did not make their way to the helper.
  if (base::win::GetVersion() >= base::win::Version::WIN10_20H1) {
    auto path = crash_reporter_client->GetWerRuntimeExceptionModule();
    if (!path.empty())
      GetCrashpadClient().RegisterWerModule(path);
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

  return true;
}

// We need to prevent ICF from folding DumpProcessForHungInputThread(),
// together, since that makes them indistinguishable in crash dumps.
// We do this by making the function body unique, and turning off inlining.
NOINLINE DWORD WINAPI DumpProcessForHungInputThread(void* param) {
  DumpWithoutCrashing();
  return 0;
}

}  // namespace internal
}  // namespace crash_reporter
