// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/cxx20_erase.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/crash/crashpad_crash_client.h"
#include "chrome/chrome_cleaner/logging/scoped_logging.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"

namespace {

// The URL where crash reports are uploaded.
const char kReportUploadURL[] = "https://clients2.google.com/cr/report";

// Whether the current process is connected to a crash handler process.
bool g_is_connected_to_crash_handler = false;

}  // namespace

crashpad::CrashpadClient* GetCrashpadClient() {
  static auto* crashpad_client = new crashpad::CrashpadClient();
  return crashpad_client;
}

void AppendSwitchIfExisting(const base::CommandLine& command_line,
                            const std::string& switch_name,
                            std::vector<std::string>* arguments) {
  if (command_line.HasSwitch(switch_name)) {
    // String format: --%s=%s
    arguments->push_back(
        base::StrCat({"--", switch_name, "=",
                      command_line.GetSwitchValueASCII(switch_name)}));
  }
}

void StartCrashReporter(const std::string version) {
  static bool started = false;
  DCHECK(!started);
  started = true;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  base::FilePath handler_path =
      chrome_cleaner::PreFetchedPaths::GetInstance()->GetExecutablePath();

  base::FilePath database_path;
  if (!chrome_cleaner::GetAppDataProductDirectory(&database_path)) {
    LOG(DFATAL) << "Failed to get AppData product directory";
    return;
  }

  std::map<std::string, std::string> annotations;  // Crash keys.
  annotations["ver"] = version;
  annotations["prod"] = "ChromeFoil";
  annotations["plat"] = "Win32";

  std::vector<std::string> arguments;
  arguments.push_back(
      base::StrCat({"--", chrome_cleaner::kCrashHandlerSwitch}));

  AppendSwitchIfExisting(*command_line, chrome_cleaner::kTestLoggingURLSwitch,
                         &arguments);
  AppendSwitchIfExisting(*command_line, chrome_cleaner::kCleanupIdSwitch,
                         &arguments);

  crashpad::CrashpadClient* client = GetCrashpadClient();
  if (!client->StartHandler(handler_path, database_path,
                            /*metrics_dir=*/base::FilePath(), kReportUploadURL,
                            annotations, arguments, /*restartable=*/true,
                            /*asynchronous_start=*/false)) {
    LOG(DFATAL) << "Failed to start handler.";
  } else {
    g_is_connected_to_crash_handler = true;
    LOG(INFO) << "Crash handler launched and ready.";
  }
}

void RemoveSwitchIfExisting(const char* const switch_to_remove,
                            std::vector<std::wstring>* argv) {
  const std::wstring pattern =
      base::StrCat({L"--", base::UTF8ToWide(switch_to_remove)});
  auto matches_switch = [&pattern](const std::wstring& argument) -> bool {
    return base::StartsWith(argument, pattern, base::CompareCase::SENSITIVE);
  };
  base::EraseIf(*argv, matches_switch);
}

int CrashReporterMain() {
  chrome_cleaner::ScopedLogging scoped_logging(L"-crashpad");

  // Make sure not to take too much of the machines's resources.
  chrome_cleaner::SetBackgroundMode();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // This function should only run if --crash-handler switch is present.
  DCHECK(command_line->HasSwitch(chrome_cleaner::kCrashHandlerSwitch));

  std::vector<std::wstring> argv = command_line->argv();

  // Because of https://bugs.chromium.org/p/crashpad/issues/detail?id=82,
  // Crashpad fails on the presence of flags it doesn't handle. Until that bug
  // is fixed, we need to remove any custom flag passed by the cleaner to the
  // Crashpad process.
  RemoveSwitchIfExisting(chrome_cleaner::kCrashHandlerSwitch, &argv);
  RemoveSwitchIfExisting(chrome_cleaner::kTestLoggingURLSwitch, &argv);
  RemoveSwitchIfExisting(chrome_cleaner::kCleanupIdSwitch, &argv);

  // Disable rate-limiting until this is fixed:
  //   https://bugs.chromium.org/p/crashpad/issues/detail?id=23
  argv.push_back(L"--no-rate-limit");

  // |storage| must be declared before |argv_as_utf8|, to ensure it outlives
  // |argv_as_utf8|, which will hold pointers into |storage|.
  std::vector<std::string> storage;
  std::unique_ptr<char* []> argv_as_utf8(new char*[argv.size() + 1]);
  storage.reserve(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
    storage.push_back(base::WideToUTF8(argv[i]));
    argv_as_utf8[i] = &storage[i][0];
  }
  argv_as_utf8[argv.size()] = nullptr;

  return crashpad::HandlerMain(static_cast<int>(argv.size()),
                               argv_as_utf8.get(),
                               /* user_stream_sources */ nullptr);
}

std::wstring GetCrashReporterIPCPipeName() {
  return g_is_connected_to_crash_handler
             ? GetCrashpadClient()->GetHandlerIPCPipe()
             : std::wstring();
}

void UseCrashReporter(const std::wstring& ipc_pipe_name) {
  DCHECK(!ipc_pipe_name.empty());
  crashpad::CrashpadClient* crashpad_client = GetCrashpadClient();
  if (!crashpad_client->SetHandlerIPCPipe(ipc_pipe_name)) {
    LOG(DFATAL) << "Failed to set handler IPC pipe name: " << ipc_pipe_name;
  } else {
    g_is_connected_to_crash_handler = true;
    LOG(INFO) << "Crash handler launched and ready.";
  }
}
