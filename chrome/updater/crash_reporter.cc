// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/crash_reporter.h"

#include <map>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/updater_constants.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"

namespace {

// True if the current process is connected to a crash handler process.
bool g_is_connected_to_crash_handler = false;

crashpad::CrashpadClient* GetCrashpadClient() {
  static auto* crashpad_client = new crashpad::CrashpadClient();
  return crashpad_client;
}

void RemoveSwitchIfExisting(const char* switch_to_remove,
                            std::vector<base::CommandLine::StringType>* argv) {
  const std::string pattern = base::StrCat({"--", switch_to_remove});
  auto matches_switch =
      [&pattern](const base::CommandLine::StringType& argument) -> bool {
#if defined(OS_WIN)
    return base::StartsWith(argument, base::UTF8ToUTF16(pattern),
                            base::CompareCase::SENSITIVE);
#else
    return base::StartsWith(argument, pattern, base::CompareCase::SENSITIVE);
#endif  // OS_WIN
  };
  base::EraseIf(*argv, matches_switch);
}

}  // namespace

namespace updater {

void StartCrashReporter(const std::string& version) {
  static bool started = false;
  DCHECK(!started);
  started = true;

  base::FilePath handler_path;
  base::PathService::Get(base::FILE_EXE, &handler_path);

  base::FilePath database_path;
  if (!GetProductDirectory(&database_path)) {
    LOG(DFATAL) << "Failed to get the database path.";
    return;
  }

  std::map<std::string, std::string> annotations;  // Crash keys.
  annotations["ver"] = version;
  annotations["prod"] = PRODUCT_FULLNAME_STRING;

  std::vector<std::string> arguments;
  arguments.push_back(base::StrCat({"--", kCrashHandlerSwitch}));

  crashpad::CrashpadClient* client = GetCrashpadClient();
  if (!client->StartHandler(handler_path, database_path,
                            /*metrics_dir=*/base::FilePath(),
                            kCrashStagingUploadURL, annotations, arguments,
                            /*restartable=*/true,
                            /*asynchronous_start=*/false)) {
    LOG(DFATAL) << "Failed to start handler.";
    return;
  }

  g_is_connected_to_crash_handler = true;
  VLOG(1) << "Crash handler launched and ready.";
}

int CrashReporterMain() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line->HasSwitch(kCrashHandlerSwitch));

  // Disable rate-limiting until this is fixed:
  //   https://bugs.chromium.org/p/crashpad/issues/detail?id=23
  command_line->AppendSwitch(kNoRateLimitSwitch);

  std::vector<base::CommandLine::StringType> argv = command_line->argv();

  // Because of https://bugs.chromium.org/p/crashpad/issues/detail?id=82,
  // Crashpad fails on the presence of flags it doesn't handle.
  RemoveSwitchIfExisting(kCrashHandlerSwitch, &argv);

  // |storage| must be declared before |argv_as_utf8|, to ensure it outlives
  // |argv_as_utf8|, which will hold pointers into |storage|.
  std::vector<std::string> storage;
  std::unique_ptr<char*[]> argv_as_utf8(new char*[argv.size() + 1]);
  storage.reserve(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
#if defined(OS_WIN)
    storage.push_back(base::UTF16ToUTF8(argv[i]));
#else
    storage.push_back(argv[i]);
#endif
    argv_as_utf8[i] = &storage[i][0];
  }
  argv_as_utf8[argv.size()] = nullptr;

  return crashpad::HandlerMain(static_cast<int>(argv.size()),
                               argv_as_utf8.get(),
                               /*user_stream_sources=*/nullptr);
}

#if defined(OS_WIN)

base::string16 GetCrashReporterIPCPipeName() {
  return g_is_connected_to_crash_handler
             ? GetCrashpadClient()->GetHandlerIPCPipe()
             : base::string16();
}

void UseCrashReporter(const base::string16& ipc_pipe_name) {
  DCHECK(!ipc_pipe_name.empty());
  crashpad::CrashpadClient* crashpad_client = GetCrashpadClient();
  if (!crashpad_client->SetHandlerIPCPipe(ipc_pipe_name)) {
    LOG(DFATAL) << "Failed to set handler IPC pipe name: " << ipc_pipe_name;
    return;
  }

  g_is_connected_to_crash_handler = true;
  VLOG(1) << "Crash handler is ready.";
}

#endif  // OS_WIN

}  // namespace updater
