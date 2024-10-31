// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_embedding_context.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

using WebUIEmbeddingContextTest = InProcessBrowserTest;

namespace webui {

IN_PROC_BROWSER_TEST_F(WebUIEmbeddingContextTest,
                       MovingTabsAcrossWindowsUpdatesBrowserInterface) {
  // Create a browser with 2 tabs.
  content::WebContents* tab_contents =
      chrome::AddAndReturnTabAt(browser(), GURL(url::kAboutBlankURL), 1, true);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(browser(), GetBrowserWindowInterface(tab_contents));

  base::MockCallback<base::RepeatingClosure> browser_changed_callback;
  base::CallbackListSubscription subscription =
      RegisterBrowserWindowInterfaceChanged(tab_contents,
                                            browser_changed_callback.Get());

  // Move the tab into a new browser window.
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  ui_test_utils::BrowserChangeObserver new_browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  chrome::MoveTabsToNewWindow(browser(), {1});
  Browser* new_browser = new_browser_observer.Wait();
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, new_browser->tab_strip_model()->count());
  EXPECT_EQ(new_browser, GetBrowserWindowInterface(tab_contents));
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback);

  // Move the tab back into its original browser window. This results in the
  // closing and destruction of the previously created browser.
  EXPECT_CALL(browser_changed_callback, Run).Times(2);
  chrome::MoveTabsToExistingWindow(new_browser, browser(), {0});
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(browser(), GetBrowserWindowInterface(tab_contents));
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback);
}

}  // namespace webui
