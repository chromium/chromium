// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

typedef BrowserWithTestWindowTest BookmarkTest;

TEST_F(BookmarkTest, NonEmptyBookmarkBarShownOnNTP) {
  profile()->CreateBookmarkModel(true);
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
  bookmarks::AddIfNotBookmarked(bookmark_model, GURL("https://www.test.com"),
                                base::string16());

  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
}

TEST_F(BookmarkTest, EmptyBookmarkBarNotShownOnNTP) {
  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
}

// Verify that the bookmark bar is hidden on custom NTP pages.
TEST_F(BookmarkTest, BookmarkBarOnCustomNTP) {
  // Create a empty commited web contents.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  web_contents->GetController().LoadURL(GURL(url::kAboutBlankURL),
                                        content::Referrer(),
                                        ui::PAGE_TRANSITION_LINK,
                                        std::string());

  // Give it a NTP virtual URL.
  content::NavigationController* controller = &web_contents->GetController();
  content::NavigationEntry* entry = controller->GetVisibleEntry();
  entry->SetVirtualURL(GURL(chrome::kChromeUINewTabURL));

  // Verify that the bookmark bar is hidden.
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  true);
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
}
