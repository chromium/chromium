// Copyright 2013 The Chromium Authors
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

#if BUILDFLAG(IS_WIN)
#include "base/win/dark_mode_support.h"
#include "base/win/win_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"  // nogncheck
#endif

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  size_t parallel_jobs = base::NumParallelJobs(/*cores_per_job=*/2);
  if (parallel_jobs == 0U) {
    return 1;
  }

#if BUILDFLAG(IS_WIN)
  // Many tests validate code that requires user32.dll to be loaded. Loading it,
  // however, cannot be done on the main thread loop because it is a blocking
  // call, and all the test code runs on the main thread loop. Instead, just
  // load and pin the module early on in startup before the blocking becomes an
  // issue.
  base::win::PinUser32();

  base::win::EnableHighDPISupport();

  // Like user32.dll above, some tests require uxtheme.dll to be loaded. This
  // call will ensure uxtheme.dll is pinned early on startup.
  base::win::IsDarkModeAvailable();
#endif  // BUILDFLAG(IS_WIN)

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Adjust switches for interactive tests where the user is expected to
  // manually verify results.
  if (command_line->HasSwitch(switches::kTestLauncherInteractive)) {
#if BUILDFLAG(IS_FUCHSIA)
    // TODO(crbug.com/1288963): Consider porting interactive tests to Fuchsia.
    LOG(FATAL) << "Interactive tests are not supported on Fuchsia.";
#endif  // BUILDFLAG(IS_FUCHSIA)

    // Since the test is interactive, the invoker will want to have pixel output
    // to actually see the result.
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
#if BUILDFLAG(IS_WIN)
    // Under Windows, dialogs (but not the browser window) created in the
    // spawned browser_test process are invisible for some unknown reason.
    // Pass in --disable-gpu to resolve this for now. See
    // http://crbug.com/687387.
    command_line->AppendSwitch(switches::kDisableGpu);
#endif  // BUILDFLAG(IS_WIN)
  }

#if BUILDFLAG(IS_FUCHSIA)
  // Running in headless mode frees the test suite from depending on
  // a graphical compositor.
  // TODO(crbug.com/1318197): Remove this extra logic once tests are run in
  // non-headless mode. See also crbug.com/1292100.
  command_line->AppendSwitch(switches::kHeadless);
  command_line->AppendSwitchNative(switches::kOzonePlatform,
                                   switches::kHeadless);

  // The default headless resolution (1x1) doesn't allow web contents to be
  // visible. That cause tests that simulate mouse clicks to fail. The size also
  // needs to accommodate some dialog tests where the window needs to be larger
  // than the dialog.
  command_line->AppendSwitchNative(switches::kOzoneOverrideScreenSize,
                                   "800,800");
#endif

  // Temporarily force the CPU backend to use AAA. (https://crbug.com/1421297)
  command_line->AppendSwitch(switches::kForceSkiaAnalyticAntialiasing);

  ChromeTestSuiteRunner runner;
  ChromeTestLauncherDelegate delegate(&runner);
  return LaunchChromeTests(parallel_jobs, &delegate, argc, argv);
}
