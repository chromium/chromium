// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_types.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/find_result_waiter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/cocoa/find_pasteboard.h"
#include "url/gurl.h"

const char kSimple[] = "simple.html";

GURL GetURL(const std::string& filename) {
  return ui_test_utils::GetTestUrl(base::FilePath().AppendASCII("find_in_page"),
                                   base::FilePath().AppendASCII(filename));
}

class FindBarPlatformHelperMacInteractiveUITest : public InProcessBrowserTest {
 public:
  FindBarPlatformHelperMacInteractiveUITest() {}
  ~FindBarPlatformHelperMacInteractiveUITest() override = default;

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

  DISALLOW_COPY_AND_ASSIGN(FindBarPlatformHelperMacInteractiveUITest);
};

// Tests that the pasteboard is updated when the find bar is changed.
IN_PROC_BROWSER_TEST_F(FindBarPlatformHelperMacInteractiveUITest,
                       PasteboardUpdatedFromFindBar) {
  FindBarController* find_bar_controller = browser()->GetFindBarController();
  ASSERT_NE(nullptr, find_bar_controller);

  const base::string16 empty_string;
  find_bar_controller->SetText(empty_string);

  chrome::Find(browser());
  EXPECT_TRUE(
      ui_test_utils::IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_S, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_D, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_F, false,
                                              false, false, false));

  base::string16 find_bar_string =
      find_bar_controller->find_bar()->GetFindText();

  ASSERT_EQ(base::ASCIIToUTF16("asdf"), find_bar_string);
  EXPECT_EQ(find_bar_string, base::SysNSStringToUTF16(
                                 [[FindPasteboard sharedInstance] findText]));
}

// Tests that the pasteboard is not updated from an incognito find bar.
IN_PROC_BROWSER_TEST_F(FindBarPlatformHelperMacInteractiveUITest,
                       IncognitoPasteboardNotUpdatedFromFindBar) {
  Browser* browser_incognito = CreateIncognitoBrowser();
  FindBarController* find_bar_controller =
      browser_incognito->GetFindBarController();
  ASSERT_NE(nullptr, find_bar_controller);

  const base::string16 empty_string;
  find_bar_controller->SetText(empty_string);

  chrome::Find(browser_incognito);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser_incognito,
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_S, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_E, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_C, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_R, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_E, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_T, false,
                                              false, false, false));

  base::string16 find_bar_string =
      find_bar_controller->find_bar()->GetFindText();

  ASSERT_EQ(base::ASCIIToUTF16("secret"), find_bar_string);
  EXPECT_NE(find_bar_string, base::SysNSStringToUTF16(
                                 [[FindPasteboard sharedInstance] findText]));
}

// Equivalent to browser_tests
// FindInPageControllerTest.GlobalPasteBoardClearMatches.
// TODO(http://crbug.com/843878): Remove when referenced bug is fixed.
// Flaky. crbug.com/864585
IN_PROC_BROWSER_TEST_F(FindBarPlatformHelperMacInteractiveUITest,
                       DISABLED_GlobalPasteBoardClearMatches) {
  FindBarController* find_bar_controller = browser()->GetFindBarController();
  ASSERT_NE(nullptr, find_bar_controller);

  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURL(browser(), url);

  const base::string16 empty_string;
  find_bar_controller->SetText(empty_string);

  chrome::Find(browser());
  EXPECT_TRUE(
      ui_test_utils::IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_P, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_G, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_E, false,
                                              false, false, false));

  EXPECT_EQ(base::ASCIIToUTF16("1/1"), find_bar_controller->find_bar()
                                           ->GetFindBarTesting()
                                           ->GetMatchCountText());

  chrome::AddTabAt(browser(), GURL(), -1, true);
  ui_test_utils::NavigateToURL(browser(), url);
  chrome::Find(browser());

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_T, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_E, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_X, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_T, false,
                                              false, false, false));

  EXPECT_EQ(base::ASCIIToUTF16("1/1"), find_bar_controller->find_bar()
                                           ->GetFindBarTesting()
                                           ->GetMatchCountText());

  // Go back to the first tab and verify that the match text is cleared.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(base::ASCIIToUTF16(""), find_bar_controller->find_bar()
                                        ->GetFindBarTesting()
                                        ->GetMatchCountText());
}

// Equivalent to browser_tests
// FindInPageControllerTest.IncognitoFindNextShared.
// TODO(http://crbug.com/843878): Remove when referenced bug is fixed.
IN_PROC_BROWSER_TEST_F(FindBarPlatformHelperMacInteractiveUITest,
                       IncognitoFindNextShared) {
  chrome::Find(browser());
  ASSERT_TRUE(
      ui_test_utils::IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_B, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_R, false,
                                              false, false, false));

  Browser* browser_incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(browser_incognito, GURL("data:text/plain,bar"));

  ASSERT_TRUE(chrome::ExecuteCommand(browser_incognito, IDC_FIND_NEXT));
  content::WebContents* web_contents_incognito =
      browser_incognito->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::FindResultWaiter(web_contents_incognito).Wait();

  FindBarController* find_bar_controller =
      browser_incognito->GetFindBarController();
  ASSERT_NE(nullptr, find_bar_controller);
  EXPECT_EQ(base::ASCIIToUTF16("bar"),
            find_bar_controller->find_bar()->GetFindText());
}

// Equivalent to browser_tests
// FindInPageControllerTest.PreferPreviousSearch.
// TODO(http://crbug.com/843878): Remove when referenced bug is fixed.
IN_PROC_BROWSER_TEST_F(FindBarPlatformHelperMacInteractiveUITest,
                       PreferPreviousSearch) {
  FindBarController* find_bar_controller = browser()->GetFindBarController();
  ASSERT_NE(nullptr, find_bar_controller);

  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* first_active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const base::string16 empty_string;
  find_bar_controller->SetText(empty_string);

  chrome::Find(browser());
  EXPECT_TRUE(
      ui_test_utils::IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_T, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_E, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_X, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_T, false,
                                              false, false, false));

  chrome::AddTabAt(browser(), GURL(), -1, true);
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_NE(first_active_web_contents,
            browser()->tab_strip_model()->GetActiveWebContents());

  chrome::Find(browser());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_G, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_I, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_V, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_E, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_N, false,
                                              false, false, false));

  // Go back to the first tab and end the search.
  browser()->tab_strip_model()->ActivateTabAt(0);
  find_bar_controller->EndFindSession(FindOnPageSelectionAction::kKeep,
                                      FindBoxResultAction::kKeep);
  // Simulate F3.
  ui_test_utils::FindInPage(first_active_web_contents, base::string16(), true,
                            false, nullptr, nullptr);
  EXPECT_EQ(
      base::ASCIIToUTF16("given"),
      FindTabHelper::FromWebContents(first_active_web_contents)->find_text());
}
