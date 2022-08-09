// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/process/process.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/updater/test/integration_test_commands.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include <memory>

#include "base/win/scoped_com_initializer.h"
#include "chrome/installer/util/scoped_token_privilege.h"
#include "chrome/updater/win/win_util.h"

namespace {

// If the priority class is not NORMAL_PRIORITY_CLASS, then the function makes:
// - the priority class of the process NORMAL_PRIORITY_CLASS
// - the process memory priority MEMORY_PRIORITY_NORMAL
// - the current thread priority THREAD_PRIORITY_NORMAL
void FixExecutionPriorities() {
  const HANDLE process = ::GetCurrentProcess();
  const DWORD priority_class = ::GetPriorityClass(process);
  if (priority_class == NORMAL_PRIORITY_CLASS)
    return;
  ::SetPriorityClass(process, NORMAL_PRIORITY_CLASS);

  static const auto set_process_information_fn =
      reinterpret_cast<decltype(&::SetProcessInformation)>(::GetProcAddress(
          ::GetModuleHandle(L"Kernel32.dll"), "SetProcessInformation"));
  if (!set_process_information_fn)
    return;
  MEMORY_PRIORITY_INFORMATION memory_priority = {};
  memory_priority.MemoryPriority = MEMORY_PRIORITY_NORMAL;
  set_process_information_fn(process, ProcessMemoryPriority, &memory_priority,
                             sizeof(memory_priority));

  ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_NORMAL);
}

void MaybeIncreaseTestTimeouts(int argc, char** argv) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kTestLauncherTimeout)) {
    command_line->AppendSwitchASCII(switches::kTestLauncherTimeout, "60000");
  }
  if (!command_line->HasSwitch(switches::kUiTestActionTimeout)) {
    command_line->AppendSwitchASCII(switches::kUiTestActionTimeout, "30000");
  }
}

}  // namespace

#endif  // BUILDFLAG(IS_WIN)

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::ScopedClosureRunner reset_command_line(
      base::BindOnce(&base::CommandLine::Reset));

#if BUILDFLAG(IS_WIN)
  std::cerr << "Process priority: " << base::Process::Current().GetPriority()
            << std::endl;
  std::cerr << updater::GetUACState() << std::endl;

  // TODO(crbug.com/1245429): remove when the bug is fixed.
  // Typically, the test suite runner expects the swarming task to run with
  // normal priority but for some reason, on the updater bots with UAC on, the
  // swarming task runs with a priority below normal.
  FixExecutionPriorities();

  // Change the test timeout defaults if the command line arguments to override
  // them are not present.
  MaybeIncreaseTestTimeouts(argc, argv);

  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);
  if (FAILED(updater::DisableCOMExceptionHandling())) {
    // Failing to disable COM exception handling is a critical error.
    CHECK(false) << "Failed to disable COM exception handling.";
  }

  installer::ScopedTokenPrivilege token_se_debug(SE_DEBUG_NAME);
  if (::IsUserAnAdmin() && !token_se_debug.is_enabled()) {
    std::cerr << "Running as administrator but can't enable SE_DEBUG_NAME."
              << std::endl;
  }

#endif

  base::TestSuite test_suite(argc, argv);
  chrome::RegisterPathProvider();
  return base::LaunchUnitTestsWithOptions(
      argc, argv, 1, 10, true, base::BindRepeating([]() {
        updater::test::CreateIntegrationTestCommands()->PrintLog();
      }),
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
