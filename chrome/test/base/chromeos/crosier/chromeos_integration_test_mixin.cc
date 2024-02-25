// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/chromeos_integration_test_mixin.h"

#include "base/command_line.h"
#include "build/chromeos_buildflags.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

ChromeOSIntegrationTestMixin::ChromeOSIntegrationTestMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

ChromeOSIntegrationTestMixin::~ChromeOSIntegrationTestMixin() = default;

void ChromeOSIntegrationTestMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  // One of the main reason for using ChromeOS integration test is it can
  // verify graphics stack code path on DUT. So we want to enable GPU and pixel
  // outputs by default. This would also be required for pixel testing
  // and generating screenshots during test failures.
  command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  command_line->AppendSwitch(switches::kUseGpuInTests);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For Ash testing, which Lacros version to use is different for Browser
  // and OS side. The strategy need to comply with the production version skew
  // guidelines.
  // On browser side, all Ash builders need to compile Lacros using an
  // alternate toolchain. Compiled Lacros is under //out/Default/lacros_clang.
  // So there is no version skew between Ash and Lacros.
  // On OS side, Ash tests should use the RootFS version packed in the
  // OS image.
  base::FilePath path = command_line->GetProgram();
  path = path.DirName().Append("lacros_clang").Append("chrome");
  if (base::PathExists(path)) {
    command_line->AppendSwitchPath(ash::switches::kLacrosChromePath, path);
    LOG(INFO) << "Testing with locally compiled Lacros.";
  } else {
    LOG(INFO) << "Testing with RootFS Lacros.";
  }
#endif
}

bool ChromeOSIntegrationTestMixin::SetUpUserDataDirectory() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Always have --user-data-dir present in commandline arguments.
  // Without the argument, there are some permission issues. Here the logic
  // is: a temporary dir is created in base framework and set in PathService.
  // Then we retrieve the dir and append it to the commandline arguments.
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (!cmdline->HasSwitch(switches::kUserDataDir)) {
    cmdline->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return InProcessBrowserTestMixin::SetUpUserDataDirectory();
}
