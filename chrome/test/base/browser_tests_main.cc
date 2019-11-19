// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "content/public/common/content_switches.h"
#include "ui/compositor/compositor_switches.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif  // defined(OS_WIN)

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  size_t parallel_jobs = base::NumParallelJobs();
  if (parallel_jobs == 0U) {
    return 1;
  } else if (parallel_jobs > 1U) {
    parallel_jobs /= 2U;
  }

#if defined(OS_WIN)
  // Many tests validate code that requires user32.dll to be loaded. Loading it,
  // however, cannot be done on the main thread loop because it is a blocking
  // call, and all the test code runs on the main thread loop. Instead, just
  // load and pin the module early on in startup before the blocking becomes an
  // issue.
  base::win::PinUser32();
#endif  // defined(OS_WIN)

  // Enable high-DPI for interactive tests where the user is expected to
  // manually verify results.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTestLauncherInteractive)) {
    // Since the test is interactive, the invoker will want to have pixel output
    // to actually see the result.
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
#if defined(OS_WIN)
    // Under Windows, dialogs (but not the browser window) created in the
    // spawned browser_test process are invisible for some unknown reason.
    // Pass in --disable-gpu to resolve this for now. See
    // http://crbug.com/687387.
    command_line->AppendSwitch(switches::kDisableGpu);

    base::win::EnableHighDPISupport();
#endif  // defined(OS_WIN)
  }

  ChromeTestSuiteRunner runner;
  ChromeTestLauncherDelegate delegate(&runner);
  return LaunchChromeTests(parallel_jobs, &delegate, argc, argv);
}
