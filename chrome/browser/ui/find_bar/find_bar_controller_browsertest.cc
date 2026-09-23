// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_controller.h"

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

using FindBarControllerTest = InProcessBrowserTest;

// Creating the FindBar on startup can result in a startup performance
// regression. This test ensures that the FindBar isn't created until
// truly needed. See https://crbug.com/41354464.
IN_PROC_BROWSER_TEST_F(FindBarControllerTest,
                       NoFindBarControllerOnBrowserCreate) {
  // FindBar should not be created on browser start.
  EXPECT_FALSE(FindBarController::From(browser())->HasFindBar());
  // find_bar() should create the FindBar on demand.
  EXPECT_NE(nullptr, FindBarController::From(browser())->find_bar());
  // This should now indicate that there is now a FindBar instance.
  EXPECT_TRUE(FindBarController::From(browser())->HasFindBar());
}

// This test ensure that the FindBar is created when the tab having
// active find session is inserted in a new window.
IN_PROC_BROWSER_TEST_F(FindBarControllerTest, FindBarControllerOnWindowCreate) {
  // Start find session.
  chrome::Find(browser());
  // Move tab to a new window
  chrome::MoveActiveTabToNewWindow(browser());

  // Make sure FindBar is created.
  EXPECT_TRUE(FindBarController::From(browser())->HasFindBar());
}
