// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_sensitivity_cache.h"

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

class TabSensitivityCacheBrowserTest : public InProcessBrowserTest {
 public:
  TabSensitivityCacheBrowserTest() = default;
  TabSensitivityCacheBrowserTest(const TabSensitivityCacheBrowserTest&) =
      delete;
  TabSensitivityCacheBrowserTest& operator=(
      const TabSensitivityCacheBrowserTest&) = delete;

  content::WebContents* AddTab(GURL url) {
    content::WebContents* contents_ptr = browser()->OpenURL(
        content::OpenURLParams(url, content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{});

    return contents_ptr;
  }

  void RemoveTab(content::WebContents* web_contents) {
    browser()->tab_strip_model()->CloseWebContentsAt(
        browser()->tab_strip_model()->GetIndexOfWebContents(web_contents),
        TabCloseTypes::CLOSE_NONE);
  }

  TabSensitivityCache* cache() { return cache_.get(); }

 private:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    cache_ = std::make_unique<TabSensitivityCache>(browser()->profile());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    cache_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<TabSensitivityCache> cache_;
};

IN_PROC_BROWSER_TEST_F(TabSensitivityCacheBrowserTest, RemembersScore) {
  const GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  AddTab(url1);
  cache()->OnPageContentAnnotated(
      url1, page_content_annotations::PageContentAnnotationsResult::
                CreateContentVisibilityScoreResult(1.0f));

  EXPECT_EQ(cache()->GetScore(url1), 0.0f);
}

IN_PROC_BROWSER_TEST_F(TabSensitivityCacheBrowserTest,
                       TrimsScoresWhenCacheIsAtLeastHalfStale) {
  // Add three tabs and remove the initial tab.
  content::WebContents* const initial_tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("b.com", "/title1.html");
  const GURL url3 = embedded_test_server()->GetURL("c.com", "/title1.html");
  content::WebContents* const tab1 = AddTab(url1);
  content::WebContents* const tab2 = AddTab(url2);
  AddTab(url3);
  cache()->OnPageContentAnnotated(
      url1, page_content_annotations::PageContentAnnotationsResult::
                CreateContentVisibilityScoreResult(1.0f));
  cache()->OnPageContentAnnotated(
      url2, page_content_annotations::PageContentAnnotationsResult::
                CreateContentVisibilityScoreResult(1.0f));
  cache()->OnPageContentAnnotated(
      url3, page_content_annotations::PageContentAnnotationsResult::
                CreateContentVisibilityScoreResult(1.0f));
  RemoveTab(initial_tab);
  ASSERT_EQ(cache()->GetScore(url1), 0.0f);

  // Remove one tab. The cache does not trim yet as it would not shrink by 1/2.
  RemoveTab(tab1);
  cache()->OnPageContentAnnotated(
      url3, page_content_annotations::PageContentAnnotationsResult::
                CreateContentVisibilityScoreResult(1.0f));
  EXPECT_EQ(cache()->GetScore(url1), 0.0f);

  // Remove one more tab. The cache trims now as it can shrink by at least 1/2.
  RemoveTab(tab2);
  cache()->OnPageContentAnnotated(
      url3, page_content_annotations::PageContentAnnotationsResult::
                CreateContentVisibilityScoreResult(1.0f));
  EXPECT_EQ(cache()->GetScore(url1), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(TabSensitivityCacheBrowserTest, ScoreNotPresent) {
  const GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");

  EXPECT_EQ(cache()->GetScore(url1), std::nullopt);
}
