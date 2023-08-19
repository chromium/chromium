// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_switches.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "url/gurl.h"

namespace ash {
namespace {

using InteractiveAshTestUITest = InteractiveAshTest;

IN_PROC_BROWSER_TEST_F(InteractiveAshTestUITest, Basics) {
  SetupContextWidget();

  // Verify that installing system apps doesn't crash or flake.
  InstallSystemApps();

  // Verify an active user exists.
  ASSERT_TRUE(GetActiveUserProfile());

  // Open a browser window.
  GURL version_url("chrome://version");
  ASSERT_TRUE(CreateBrowserWindow(version_url));

  // Open a second browser window.
  GURL blank_url("about:blank");
  ASSERT_TRUE(CreateBrowserWindow(blank_url));

  // You don't need this for your tests. This is just to prevent the test from
  // exiting so you can play with the browser windows.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherInteractive)) {
    base::RunLoop loop;
    loop.Run();
  }
}

}  // namespace
}  // namespace ash
