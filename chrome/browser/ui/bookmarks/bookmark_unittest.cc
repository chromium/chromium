// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

class BookmarkTest : public BrowserWithTestWindowTest {
 public:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{BookmarkModelFactory::GetInstance(),
             BookmarkModelFactory::GetDefaultFactory()}};
  }
};

TEST_F(BookmarkTest, NonEmptyBookmarkBarShownOnNTP) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  bookmarks::AddIfNotBookmarked(bookmark_model, GURL("https://www.test.com"),
                                std::u16string());

  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
}

TEST_F(BookmarkTest, EmptyBookmarkBarNotShownOnNTP) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

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

TEST_F(BookmarkTest, BookmarkReaderModePageActuallyBookmarksOriginal) {
  GURL original("https://www.example.com/article.html");
  GURL distilled = dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      dom_distiller::kDomDistillerScheme, original, "Article title");
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  web_contents->GetController().LoadURL(
      distilled, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  // The URL to bookmark and the title of the page should be based on the
  // original page.
  GURL bookmarked_url;
  std::u16string bookmarked_title;
  chrome::GetURLAndTitleToBookmark(web_contents.get(), &bookmarked_url,
                                   &bookmarked_title);
  EXPECT_EQ(original, bookmarked_url);
  EXPECT_EQ(u"Article title", bookmarked_title);
}
