// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/shopping_ui_handler_delegate.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

// Tests ShoppingUiHandlerDelegate.
class ShoppingUiHandlerDelegateBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    profile_ = Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(profile_);
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateToURL(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    base::PlatformThread::Sleep(base::Seconds(2));
    base::RunLoop().RunUntilIdle();
  }

  void OpenURLInNewTab(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
            ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    base::PlatformThread::Sleep(base::Seconds(2));
    base::RunLoop().RunUntilIdle();
  }

  raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<bookmarks::BookmarkModel, DanglingUntriaged> bookmark_model_;
};

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestGetCurrentUrl) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(nullptr, profile_);
  const GURL url = GURL("https://www.example.com");
  NavigateToURL(url);

  ASSERT_TRUE(delegate->GetCurrentTabUrl().has_value());
  ASSERT_EQ(delegate->GetCurrentTabUrl().value(), url);
}

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestGetBookmarkForCurrentUrl) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(nullptr, profile_);
  const GURL url = GURL("https://www.example.com");
  NavigateToURL(url);

  const bookmarks::BookmarkNode* other_node = bookmark_model_->other_node();
  auto* existing_node = bookmark_model_->AddNewURL(
      other_node, other_node->children().size(), u"test", url);
  size_t bookmark_count = other_node->children().size();

  auto* node = delegate->GetOrAddBookmarkForCurrentUrl();
  ASSERT_EQ(existing_node->id(), node->id());
  ASSERT_EQ(bookmark_count, other_node->children().size());
}

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestAddBookmarkForCurrentUrl) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(nullptr, profile_);
  const GURL url = GURL("https://www.example.com");
  NavigateToURL(url);

  const bookmarks::BookmarkNode* other_node = bookmark_model_->other_node();
  size_t bookmark_count = other_node->children().size();

  auto* node = delegate->GetOrAddBookmarkForCurrentUrl();

  DCHECK(node);
  ASSERT_EQ(bookmark_count + 1, other_node->children().size());
}

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestSwitchToOrOpenTab_SwitchToExistingTab) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(nullptr, profile_);
  const GURL url_1 = GURL("https://www.example.com");
  NavigateToURL(url_1);
  const auto* web_contents_1 = web_contents();
  const GURL url_2 = GURL("https://www.google.com");
  OpenURLInNewTab(url_2);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_NE(web_contents(), web_contents_1);
  ASSERT_EQ(url_2, web_contents()->GetLastCommittedURL());
  delegate->SwitchToOrOpenTab(url_1);

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(web_contents_1, web_contents());
  EXPECT_EQ(url_1, web_contents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestSwitchToOrOpenTab_OpenNewTab) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(nullptr, profile_);
  const GURL url = GURL("https://www.example.com");
  NavigateToURL(url);
  const GURL url_2 = GURL("https://www.google.com");
  content::TestNavigationObserver observer(url_2);
  observer.StartWatchingNewWebContents();

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(url, web_contents()->GetLastCommittedURL());
  delegate->SwitchToOrOpenTab(url_2);
  observer.WaitForNavigationFinished();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(url_2, web_contents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestSwitchToOrOpenTab_InvalidUrls) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(nullptr, profile_);
  const GURL invalid_url_1 = GURL("chrome://newtab");
  NavigateToURL(invalid_url_1);
  const GURL invalid_url_2 = GURL("file://foo");
  OpenURLInNewTab(invalid_url_2);
  const GURL valid_url = GURL("https://www.example.com");
  OpenURLInNewTab(valid_url);
  const auto* valid_web_contents = web_contents();

  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_EQ(valid_web_contents, web_contents());
  ASSERT_EQ(valid_url, web_contents()->GetLastCommittedURL());
  delegate->SwitchToOrOpenTab(invalid_url_1);

  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  // Ensure that the web contents remain the same, since `SwitchToOrOpenTab`
  // shouldn't work for non-HTTP(S) urls.
  EXPECT_EQ(valid_web_contents, web_contents());
  EXPECT_EQ(valid_url, web_contents()->GetLastCommittedURL());

  delegate->SwitchToOrOpenTab(invalid_url_2);

  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(valid_web_contents, web_contents());
  EXPECT_EQ(valid_url, web_contents()->GetLastCommittedURL());
}
