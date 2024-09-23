// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
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
    return {TestingProfile::TestingFactory{
        BookmarkModelFactory::GetInstance(),
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
  bool r = chrome::GetURLAndTitleToBookmark(web_contents.get(), &bookmarked_url,
                                            &bookmarked_title);
  ASSERT_TRUE(r);
  EXPECT_EQ(original, bookmarked_url);
  EXPECT_EQ(u"Article title", bookmarked_title);
}

TEST_F(BookmarkTest, NoTabsInGroups) {
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::AddFolder(nullptr, 10);

  std::vector<std::pair<GURL, std::u16string>> tab_entries;
  auto test_url =
      std::make_pair(GURL("https://www.example.com/article.html"), u"");
  base::flat_map<int, chrome::TabGroupData> groups_by_index;
  for (int i = 0; i < 6; i++) {
    tab_entries.push_back(test_url);
    groups_by_index.emplace(i, std::make_pair(std::nullopt, u""));
  }

  chrome::GetURLsAndFoldersForTabEntries(&(details.bookmark_data.children),
                                         tab_entries, groups_by_index);

  EXPECT_EQ(details.bookmark_data.children.size(), 6u);
  for (auto child : details.bookmark_data.children) {
    EXPECT_EQ(child.url.has_value(), true);
  }
}

TEST_F(BookmarkTest, AllTabsInOneGroup) {
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::AddFolder(nullptr, 10);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  std::vector<std::pair<GURL, std::u16string>> tab_entries;
  auto test_url =
      std::make_pair(GURL("https://www.example.com/article.html"), u"");
  base::flat_map<int, chrome::TabGroupData> groups_by_index;
  for (int i = 0; i < 6; i++) {
    tab_entries.push_back(test_url);
    groups_by_index.emplace(i,
                            std::make_pair(std::make_optional(group_id), u""));
  }

  chrome::GetURLsAndFoldersForTabEntries(&(details.bookmark_data.children),
                                         tab_entries, groups_by_index);

  EXPECT_EQ(details.bookmark_data.children.size(), 1u);
  EXPECT_EQ(details.bookmark_data.children.begin()->url.has_value(), false);
  EXPECT_EQ(details.bookmark_data.children.begin()->children.size(), 6u);
}

TEST_F(BookmarkTest, AllTabsInMultipleGroups) {
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::AddFolder(nullptr, 10);

  std::vector<std::pair<GURL, std::u16string>> tab_entries;
  auto test_url =
      std::make_pair(GURL("https://www.example.com/article.html"), u"");
  base::flat_map<int, chrome::TabGroupData> groups_by_index;
  for (int i = 0; i < 6; i++) {
    tab_entries.push_back(test_url);
    groups_by_index.emplace(
        i, std::make_pair(
               std::make_optional(tab_groups::TabGroupId::GenerateNew()), u""));
  }

  chrome::GetURLsAndFoldersForTabEntries(&(details.bookmark_data.children),
                                         tab_entries, groups_by_index);

  EXPECT_EQ(details.bookmark_data.children.size(), 6u);
  for (auto child : details.bookmark_data.children) {
    EXPECT_EQ(child.url.has_value(), false);
    EXPECT_EQ(child.children.size(), 1u);
  }
}

TEST_F(BookmarkTest, SomeTabsInOneGroup) {
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::AddFolder(nullptr, 10);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  std::vector<std::pair<GURL, std::u16string>> tab_entries;
  auto test_url =
      std::make_pair(GURL("https://www.example.com/article.html"), u"");
  base::flat_map<int, chrome::TabGroupData> groups_by_index;
  for (int i = 0; i < 6; i++) {
    tab_entries.push_back(test_url);
    groups_by_index.emplace(
        i, std::make_pair(
               i >= 1 && i <= 3 ? std::make_optional(group_id) : std::nullopt,
               u""));
  }

  chrome::GetURLsAndFoldersForTabEntries(&(details.bookmark_data.children),
                                         tab_entries, groups_by_index);

  EXPECT_EQ(details.bookmark_data.children.size(), 4u);
  for (size_t i = 0; i < details.bookmark_data.children.size(); i++) {
    auto child = details.bookmark_data.children.at(i);
    if (i == 1) {
      EXPECT_EQ(child.url.has_value(), false);
      EXPECT_EQ(child.children.size(), 3u);
    } else {
      EXPECT_EQ(child.url.has_value(), true);
    }
  }
}

TEST_F(BookmarkTest, SomeTabsInMultipleGroups) {
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::AddFolder(nullptr, 10);

  std::vector<std::pair<GURL, std::u16string>> tab_entries;
  auto test_url =
      std::make_pair(GURL("https://www.example.com/article.html"), u"");
  base::flat_map<int, chrome::TabGroupData> groups_by_index;
  for (int i = 0; i < 6; i++) {
    tab_entries.push_back(test_url);
    groups_by_index.emplace(
        i, std::make_pair(
               i % 2 == 0
                   ? std::make_optional(tab_groups::TabGroupId::GenerateNew())
                   : std::nullopt,
               u""));
  }

  chrome::GetURLsAndFoldersForTabEntries(&(details.bookmark_data.children),
                                         tab_entries, groups_by_index);

  EXPECT_EQ(details.bookmark_data.children.size(), 6u);
  for (size_t i = 0; i < details.bookmark_data.children.size(); i++) {
    auto child = details.bookmark_data.children.at(i);
    if (i % 2 == 0) {
      EXPECT_EQ(child.url.has_value(), false);
      EXPECT_EQ(child.children.size(), 1u);
    } else {
      EXPECT_EQ(child.url.has_value(), true);
    }
  }
}
