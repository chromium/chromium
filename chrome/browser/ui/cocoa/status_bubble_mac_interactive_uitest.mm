// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace {

class StatusBubbleMacInteractiveUITest : public InProcessBrowserTest {
 public:
  StatusBubbleMacInteractiveUITest() {}
};

// interactive_ui_tests brings browser window to front using
// ui_test_utils::BringBrowserWindowToFront, which cause NSApp hide: and unhide:
// do not work properly.
IN_PROC_BROWSER_TEST_F(StatusBubbleMacInteractiveUITest,
                       DISABLED_TestSettingStatusDoesNotUnhideApp) {
  StatusBubble* status_bubble = browser()->window()->GetStatusBubble();

  EXPECT_FALSE(NSApp.hidden);
  [NSApp hide:nil];
  EXPECT_TRUE(NSApp.hidden);
  status_bubble->SetStatus(base::UTF8ToUTF16("Testing"));
  EXPECT_TRUE(NSApp.hidden);
  [NSApp unhide:nil];
  EXPECT_FALSE(NSApp.hidden);
}

}  // namespace
