// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/test/base/find_result_waiter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/cocoa/find_pasteboard.h"

const char kSimple[] = "simple.html";

GURL GetURL(const std::string& filename) {
  return ui_test_utils::GetTestUrl(base::FilePath().AppendASCII("find_in_page"),
                                   base::FilePath().AppendASCII(filename));
}

int WaitForFind(content::WebContents* web_contents, int* ordinal) {
  ui_test_utils::FindResultWaiter observer(web_contents);
  observer.Wait();
  if (ordinal) {
    *ordinal = observer.active_match_ordinal();
  }
  return observer.number_of_matches();
}

class FindBarPlatformHelperMacTest : public InProcessBrowserTest {
 public:
  FindBarPlatformHelperMacTest() {}

  FindBarPlatformHelperMacTest(const FindBarPlatformHelperMacTest&) = delete;
  FindBarPlatformHelperMacTest& operator=(const FindBarPlatformHelperMacTest&) =
      delete;

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

// Tests that the find bar text and results are updated as the global find
// pasteboard updates. The following bug used to exist:
// 1) Find some text.
// 2) Switch to another app and find something there to put it in the global
//    find pasteboard.
// 3) Switch back to chrome, make sure no text is selected, and open the
//    findbar. The bug caused the match counts from the previous search to
//    remain in the findbar and the old find results to remain highlighted.
IN_PROC_BROWSER_TEST_F(FindBarPlatformHelperMacTest,
                       FindBarUpdatedFromPasteboard) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL(kSimple)));

  FindBarController* find_bar_controller = browser()->GetFindBarController();
  ASSERT_NE(nullptr, find_bar_controller);
  FindBar* find_bar = find_bar_controller->find_bar();
  ASSERT_NE(nullptr, find_bar);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, web_contents);
  find_in_page::FindTabHelper* helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents);
  ASSERT_NE(nullptr, helper);

  int ordinal = -1;
  int find_request_id = helper->current_find_request_id();
  NSString* update_string = @"Update String";
  [[FindPasteboard sharedInstance] setFindText:update_string];

  // setFindText shouldn't trigger a find since the findbar isn't showing
  EXPECT_EQ(find_request_id, helper->current_find_request_id());
  EXPECT_EQ(base::SysNSStringToUTF16(update_string), find_bar->GetFindText());

  find_bar_controller->Show();
  // Showing the findbar should trigger a find request
  EXPECT_EQ(find_request_id + 1, helper->current_find_request_id());
  EXPECT_EQ(0, WaitForFind(web_contents, &ordinal));
  EXPECT_EQ(0, ordinal);

  NSString* next_string = @"some text";
  [[FindPasteboard sharedInstance] setFindText:next_string];
  // setFindText should trigger a find since the findbar is now showing
  EXPECT_EQ(find_request_id + 2, helper->current_find_request_id());
  EXPECT_EQ(1, WaitForFind(web_contents, &ordinal));
  EXPECT_EQ(0, ordinal);
  EXPECT_EQ(base::SysNSStringToUTF16(next_string), find_bar->GetFindText());

  NSString* empty_string = @"";
  [[FindPasteboard sharedInstance] setFindText:empty_string];
  // setting an empty string won't trigger a find, but it should clear
  // the find results
  EXPECT_EQ(find_request_id + 2, helper->current_find_request_id());
  EXPECT_EQ(-1, helper->find_result().number_of_matches());
  EXPECT_EQ(-1, helper->find_result().active_match_ordinal());
  EXPECT_EQ(std::u16string(),
            find_bar->GetFindBarTesting()->GetMatchCountText());
  EXPECT_EQ(base::SysNSStringToUTF16(empty_string), find_bar->GetFindText());
}
