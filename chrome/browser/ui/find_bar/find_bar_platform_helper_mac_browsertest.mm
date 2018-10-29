// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/cocoa/find_pasteboard.h"

class FindBarPlatformHelperMacTest : public InProcessBrowserTest {
 public:
  FindBarPlatformHelperMacTest() {}
  ~FindBarPlatformHelperMacTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    old_find_text_ = [[FindPasteboard sharedInstance] findText];
  }

  void TearDownOnMainThread() override {
    [[FindPasteboard sharedInstance] setFindText:old_find_text_];
    InProcessBrowserTest::TearDownOnMainThread();
  }

 private:
  NSString* old_find_text_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FindBarPlatformHelperMacTest);
};

// Tests that the find bar is populated with the pasteboard at construction.
IN_PROC_BROWSER_TEST_F(FindBarPlatformHelperMacTest,
                       FindBarPopulatedWithPasteboardOnConstruction) {
  ASSERT_FALSE(browser()->HasFindBarController());

  NSString* initial_find_string = @"Initial String";
  [[FindPasteboard sharedInstance] setFindText:initial_find_string];

  FindBarController* find_bar_controller = browser()->GetFindBarController();
  ASSERT_NE(nullptr, find_bar_controller);

  EXPECT_EQ(base::SysNSStringToUTF16(initial_find_string),
            find_bar_controller->find_bar()->GetFindText());
}

// Tests that the find bar is updated as the pasteboard updates.
IN_PROC_BROWSER_TEST_F(FindBarPlatformHelperMacTest,
                       FindBarUpdatedFromPasteboard) {
  FindBarController* find_bar_controller = browser()->GetFindBarController();
  ASSERT_NE(nullptr, find_bar_controller);

  NSString* update_string = @"Update String";
  [[FindPasteboard sharedInstance] setFindText:update_string];

  EXPECT_EQ(base::SysNSStringToUTF16(update_string),
            find_bar_controller->find_bar()->GetFindText());

  NSString* next_string = @"Next String";
  [[FindPasteboard sharedInstance] setFindText:next_string];

  EXPECT_EQ(base::SysNSStringToUTF16(next_string),
            find_bar_controller->find_bar()->GetFindText());
}
