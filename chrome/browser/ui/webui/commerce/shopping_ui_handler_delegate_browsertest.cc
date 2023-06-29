// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/commerce/shopping_ui_handler_delegate.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "content/public/test/browser_test.h"

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

  void NavigateToURL(
      const GURL& url,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, disposition,
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
