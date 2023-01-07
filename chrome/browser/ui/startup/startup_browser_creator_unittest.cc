// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif

TEST(StartupBrowserCreatorTest, ShouldLoadProfileWithoutWindow) {
  {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Forcibly set ash-chrome as the primary browser.
    // This is the current default behavior.
    auto set_lacros_primary =
        crosapi::browser_util::SetLacrosPrimaryBrowserForTest(false);
#endif
    EXPECT_FALSE(StartupBrowserCreator::ShouldLoadProfileWithoutWindow(
        base::CommandLine(base::CommandLine::NO_PROGRAM)));
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(switches::kNoStartupWindow);
    EXPECT_TRUE(
        StartupBrowserCreator::ShouldLoadProfileWithoutWindow(command_line));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  {
    // Check what happens if lacros-chrome becomes the primary browser.
    auto set_lacros_primary =
        crosapi::browser_util::SetLacrosPrimaryBrowserForTest(true);
    EXPECT_TRUE(StartupBrowserCreator::ShouldLoadProfileWithoutWindow(
        base::CommandLine(base::CommandLine::NO_PROGRAM)));
  }
#endif
}
