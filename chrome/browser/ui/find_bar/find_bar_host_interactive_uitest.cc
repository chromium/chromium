// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
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
                      std::string_view search_str,
                      bool forward,
                      bool case_sensitive,
                      int* ordinal) {
    std::u16string search_str16(base::ASCIIToUTF16(search_str));
    Browser* browser = chrome::FindBrowserWithTab(web_contents);
    browser->GetFindBarController()->find_bar()->SetFindTextAndSelectedRange(
        search_str16, gfx::Range());
    return ui_test_utils::FindInPage(web_contents, search_str16, forward,
                                     case_sensitive, ordinal, nullptr);
  }
};

}  // namespace

[[nodiscard]] std::string FocusedOnPage(WebContents* web_contents) {
  return content::EvalJs(web_contents, "getFocusedElement();").ExtractString();
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents);

  // Verify that nothing has focus.
  ASSERT_EQ("{nothing focused}", FocusedOnPage(web_contents));

  // Search for a text that exists within a link on the page.
  int ordinal = 0;
  EXPECT_EQ(1, FindInPageASCII(web_contents, "nk",
                               true, false, &ordinal));
  EXPECT_EQ(1, ordinal);

  // End the find session, which should set focus to the link.
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);

  // Verify that the link is focused.
  EXPECT_EQ("link1", FocusedOnPage(web_contents));

  // Search for a text that exists within a link on the page.
  EXPECT_EQ(1, FindInPageASCII(web_contents, "Google",
                               true, false, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Move the selection to link 1, after searching.
  EXPECT_TRUE(content::ExecJs(web_contents, "selectLink1();"));

  // End the find session.
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);

  // Verify that link2 is not focused.
  EXPECT_EQ("", FocusedOnPage(web_contents));
}
