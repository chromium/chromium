// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/native_window_tracker.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

typedef InProcessBrowserTest NativeWindowTrackerTest;

IN_PROC_BROWSER_TEST_F(NativeWindowTrackerTest, Basic) {
  // Create a second browser to prevent the app from exiting when the browser is
  // closed.
  CreateBrowser(browser()->profile());

  std::unique_ptr<NativeWindowTracker> tracker =
      NativeWindowTracker::Create(browser()->window()->GetNativeWindow());
  EXPECT_FALSE(tracker->WasNativeWindowClosed());

  browser()->window()->Close();
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(tracker->WasNativeWindowClosed());
}
