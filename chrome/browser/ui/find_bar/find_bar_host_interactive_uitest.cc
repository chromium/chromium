// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_types.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using base::WideToUTF16;
using content::WebContents;

namespace {

const char kEndState[] = "/find_in_page/end_state.html";

class FindInPageInteractiveTest : public InProcessBrowserTest {
 public:
  FindInPageInteractiveTest() {
  }

  // Platform independent FindInPage that takes |const wchar_t*|
  // as an input.
  int FindInPageASCII(WebContents* web_contents,
                      const base::StringPiece& search_str,
                      bool forward,
                      bool case_sensitive,
                      int* ordinal) {
    base::string16 search_str16(ASCIIToUTF16(search_str));
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    browser->GetFindBarController()->find_bar()->SetFindTextAndSelectedRange(
        search_str16, gfx::Range());
    return ui_test_utils::FindInPage(
        web_contents, search_str16, forward, case_sensitive, ordinal, NULL);
  }
};

}  // namespace

// Specifying a prototype so that we can add the WARN_UNUSED_RESULT attribute.
bool FocusedOnPage(WebContents* web_contents, std::string* result)
    WARN_UNUSED_RESULT;

bool FocusedOnPage(WebContents* web_contents, std::string* result) {
  return content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(getFocusedElement());",
      result);
}

// This tests the FindInPage end-state, in other words: what is focused when you
// close the Find box (ie. if you find within a link the link should be
// focused).
IN_PROC_BROWSER_TEST_F(FindInPageInteractiveTest, FindInPageEndState) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // First we navigate to our special focus tracking page.
  GURL url = embedded_test_server()->GetURL(kEndState);
  ui_test_utils::NavigateToURL(browser(), url);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NULL != web_contents);
  FindTabHelper* find_tab_helper =
      FindTabHelper::FromWebContents(web_contents);

  // Verify that nothing has focus.
  std::string result;
  ASSERT_TRUE(FocusedOnPage(web_contents, &result));
  ASSERT_STREQ("{nothing focused}", result.c_str());

  // Search for a text that exists within a link on the page.
  int ordinal = 0;
  EXPECT_EQ(1, FindInPageASCII(web_contents, "nk",
                               true, false, &ordinal));
  EXPECT_EQ(1, ordinal);

  // End the find session, which should set focus to the link.
  find_tab_helper->StopFinding(FindOnPageSelectionAction::kKeep);

  // Verify that the link is focused.
  ASSERT_TRUE(FocusedOnPage(web_contents, &result));
  EXPECT_STREQ("link1", result.c_str());

  // Search for a text that exists within a link on the page.
  EXPECT_EQ(1, FindInPageASCII(web_contents, "Google",
                               true, false, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Move the selection to link 1, after searching.
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(selectLink1());",
      &result));

  // End the find session.
  find_tab_helper->StopFinding(FindOnPageSelectionAction::kKeep);

  // Verify that link2 is not focused.
  ASSERT_TRUE(FocusedOnPage(web_contents, &result));
  EXPECT_STREQ("", result.c_str());
}
