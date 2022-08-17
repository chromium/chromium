// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/unwind_util.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(UnwindPrerequisitesDeathTest, CannotInstallOutsideBrowser) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kProcessType, switches::kRendererProcess);
  ASSERT_DEATH_IF_SUPPORTED(UnwindPrerequisites::RequestInstallation(), "");
}

TEST(UnwindPrerequisitesTest, CanInstallInsideBrowser) {
  // No process type switch implies browser process.
  *base::CommandLine::ForCurrentProcess() =
      base::CommandLine(base::CommandLine::NO_PROGRAM);
  UnwindPrerequisites::RequestInstallation();
}
