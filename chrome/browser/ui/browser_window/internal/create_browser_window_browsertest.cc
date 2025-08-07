// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using CreateBrowserWindowBrowserTest = InProcessBrowserTest;

// Smoke test for CreateBrowserWindow(). Since CreateBrowserWindow() really just
// forwards to Browser::Create(), we don't exercise it in much depth yet.
IN_PROC_BROWSER_TEST_F(CreateBrowserWindowBrowserTest, CreateNewBrowserWindow) {
  BrowserWindowInterface* new_browser =
      CreateBrowserWindow(BrowserWindowCreateParams(
          *browser()->profile(), /*from_user_gesture=*/false));

  ASSERT_TRUE(new_browser);
  ASSERT_NE(new_browser, browser());
  EXPECT_EQ(new_browser->GetProfile(), browser()->profile());
  EXPECT_EQ(BrowserWindowInterface::TYPE_NORMAL, new_browser->GetType());
}
