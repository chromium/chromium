// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/crash_client.h"
#include "chrome/enterprise_companion/enterprise_companion.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace enterprise_companion {
namespace {
constexpr char kCrashDatabaseSwitch[] = "crash-database-path";

int TestCrashpadEmbedderMain(int argc, const char* const* argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(kLoggingModuleSwitch, "*=2");
  logging::InitLogging({.logging_dest = logging::LOG_TO_STDERR});
  base::AtExitManager exit_manager;

  if (command_line->HasSwitch(kCrashHandlerSwitch)) {
    return enterprise_companion::CrashReporterMain();
  }

  if (!command_line->HasSwitch(kCrashDatabaseSwitch)) {
    LOG(ERROR) << "Missing switch: " << kCrashDatabaseSwitch;
    return 1;
  }

  enterprise_companion::InitializeCrashReporting(
      command_line->GetSwitchValuePath(kCrashDatabaseSwitch));
  CHECK(false) << "Intentional crash for testing";
  return 0;
}

}  // namespace

}  // namespace enterprise_companion

#if BUILDFLAG(IS_POSIX)
int main(int argc, const char* argv[]) {
  return enterprise_companion::TestCrashpadEmbedderMain(argc, argv);
}
#elif BUILDFLAG(IS_WIN)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE prev, wchar_t*, int) {
  // `argc` and `argv` are ignored by `base::CommandLine` for Windows. Instead,
  // the implementation parses `GetCommandLineW()` directly.
  return enterprise_companion::TestCrashpadEmbedderMain(0, nullptr);
}
#endif
