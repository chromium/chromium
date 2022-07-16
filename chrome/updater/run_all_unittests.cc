// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/bind.h"
#include "base/process/process.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"

#if defined(OS_WIN)
#include <memory>

#include "base/win/scoped_com_initializer.h"
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

}  // namespace

#endif  // OS_WIN

int main(int argc, char** argv) {
#if defined(OS_WIN)
  std::cerr << "Process priority: " << base::Process::Current().GetPriority()
            << std::endl;
  std::cerr << updater::GetUACState() << std::endl;

  // TODO(crbug.com/1245429): remove when the bug is fixed.
  // Typically, the test suite runner expects the swarming task to run with
  // normal priority but for some reason, on the updater bots with UAC on, the
  // swarming task runs with a priority below normal.
  FixExecutionPriorities();

  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);
#endif
  base::TestSuite test_suite(argc, argv);
  chrome::RegisterPathProvider();
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
