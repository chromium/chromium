// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(StartupBrowserCreatorTest, ShouldLoadProfileWithoutWindow) {
  {
    EXPECT_FALSE(StartupBrowserCreator::ShouldLoadProfileWithoutWindow(
        base::CommandLine(base::CommandLine::NO_PROGRAM)));
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(switches::kNoStartupWindow);
    EXPECT_TRUE(
        StartupBrowserCreator::ShouldLoadProfileWithoutWindow(command_line));
  }
}
