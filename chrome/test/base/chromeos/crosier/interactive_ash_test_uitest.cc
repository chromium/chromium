// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "url/gurl.h"

namespace ash {
namespace {

class InteractiveAshTestUITest : public InteractiveAshTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveAshTest::SetUpCommandLine(command_line);
    SetUpCommandLineForLacros(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(InteractiveAshTestUITest, Basics) {
  SetupContextWidget();

  // Verify that installing system apps doesn't crash or flake.
  InstallSystemApps();

  // Verify that the Wayland server starts and doesn't crash or flake.
  WaitForAshFullyStarted();

  // Verify an active user exists.
  ASSERT_TRUE(GetActiveUserProfile());

  // Open a browser window.
  GURL version_url("chrome://version");
  ASSERT_TRUE(CreateBrowserWindow(version_url));

  // Open a second browser window.
  GURL blank_url("about:blank");
  ASSERT_TRUE(CreateBrowserWindow(blank_url));
}

}  // namespace
}  // namespace ash
