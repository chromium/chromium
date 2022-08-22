// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests for the glanceables feature, which adds a "welcome back" screen on
// some logins.
class GlanceablesBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    // The test harness adds --no-first-run. Remove it so glanceables show up.
    command_line->RemoveSwitch(switches::kNoFirstRun);
  }

 protected:
  base::test::ScopedFeatureList features_{ash::features::kGlanceables};
};

IN_PROC_BROWSER_TEST_F(GlanceablesBrowserTest, ShowsOnLogin) {
  EXPECT_TRUE(ash::Shell::Get()->glanceables_controller()->IsShowing());
}
