// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

using FindBarControllerTest = InProcessBrowserTest;

// Creating the FindBarController on startup can result in a startup performance
// regression. This test ensures that the FindBarController isn't created until
// truly needed. See https://crbug.com/783350.
IN_PROC_BROWSER_TEST_F(FindBarControllerTest,
                       NoFindBarControllerOnBrowserCreate) {
  // FindBarController should not be created on browser start.
  EXPECT_FALSE(browser()->HasFindBarController());
  // GetFindBarController should create the FindBarController on demand.
  EXPECT_NE(nullptr, browser()->GetFindBarController());
  // This should now indicate that there is now a FindBarController instance.
  EXPECT_TRUE(browser()->HasFindBarController());
}
