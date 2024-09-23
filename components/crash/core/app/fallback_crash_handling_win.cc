// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/fallback_crash_handling_win.h"

#include <memory>

#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "components/app_launch_prefetch/app_launch_prefetch.h"
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/fallback_crash_handler_launcher_win.h"
#include "components/crash/core/app/fallback_crash_handler_win.h"

namespace crash_reporter {

namespace switches {
const char kFallbackCrashHandler[] = "fallback-handler";
}

const uint32_t kFallbackCrashTerminationCode = 0xFFFF8001;

namespace {

// Intentionally leaked on program exit.
FallbackCrashHandlerLauncher* g_fallback_crash_handler_launcher = nullptr;

LONG WINAPI FallbackUnhandledExceptionFilter(EXCEPTION_POINTERS* exc_ptrs) {
  if (!g_fallback_crash_handler_launcher)
    return EXCEPTION_CONTINUE_SEARCH;

  return g_fallback_crash_handler_launcher->LaunchAndWaitForHandler(exc_ptrs);
}

}  // namespace

bool SetupFallbackCrashHandling(const base::CommandLine& command_line) {
  DCHECK(!g_fallback_crash_handler_launcher);

  // Run the same program.
  base::CommandLine base_command_line(command_line.GetProgram());
  base_command_line.AppendSwitchASCII("type", switches::kFallbackCrashHandler);

  // This is to support testing under gtest.
  if (command_line.HasSwitch(::switches::kTestChildProcess)) {
    base_command_line.AppendSwitchASCII(
        ::switches::kTestChildProcess,
        command_line.GetSwitchValueASCII(::switches::kTestChildProcess));
  }

  // All Chrome processes need a prefetch argument.
  base_command_line.AppendArgNative(app_launch_prefetch::GetPrefetchSwitch(
      app_launch_prefetch::SubprocessType::kCrashpadFallback));

  // Get the database path.
  base::FilePath database_path = command_line.GetSwitchValuePath("database");
  if (database_path.empty()) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  std::unique_ptr<FallbackCrashHandlerLauncher> fallback_launcher(
      new FallbackCrashHandlerLauncher());

  if (!fallback_launcher->Initialize(base_command_line, database_path)) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  // This is necessary because chrome_elf stubs out the
  // SetUnhandledExceptionFilter in the IAT of chrome.exe.
  using SetUnhandledExceptionFilterFunction =
      PTOP_LEVEL_EXCEPTION_FILTER(WINAPI*)(PTOP_LEVEL_EXCEPTION_FILTER filter);
  HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");
  if (!kernel32)
    return false;

  SetUnhandledExceptionFilterFunction set_unhandled_exception_filter =
      reinterpret_cast<SetUnhandledExceptionFilterFunction>(
          GetProcAddress(kernel32, "SetUnhandledExceptionFilter"));
  if (!set_unhandled_exception_filter)
    return false;

  // Success, pass ownership to the global.
  g_fallback_crash_handler_launcher = fallback_launcher.release();

  set_unhandled_exception_filter(&FallbackUnhandledExceptionFilter);

  return true;
}

int RunAsFallbackCrashHandler(const base::CommandLine& command_line,
                              std::string product_name,
                              std::string version,
                              std::string channel_name) {
  FallbackCrashHandler fallback_handler;

  if (!fallback_handler.ParseCommandLine(command_line)) {
    // TODO(siggi): Figure out how to UMA from this process, if need be.
    return 1;
  }

  if (!fallback_handler.GenerateCrashDump(
          product_name, version, channel_name,
          crash_reporter::switches::kCrashpadHandler)) {
    // TODO(siggi): Figure out how to UMA from this process, if need be.
    return 2;
  }

  if (!fallback_handler.process().Terminate(kFallbackCrashTerminationCode,
                                            false)) {
    return 3;
  }

  return 0;
}

}  // namespace crash_reporter
