// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

class BookmarkTest : public BrowserWithTestWindowTest {
 public:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {TestingProfile::TestingFactory{
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory()}};
  }

  void AddGroup(const std::u16string& title,
                tab_groups::TabGroupSyncService* service) {
    tab_groups::SavedTabGroup group(
        title, tab_groups::TabGroupColorId::kGrey, {}, std::nullopt,
        base::Uuid::GenerateRandomV4(), std::nullopt);
    service->AddGroup(std::move(group));
  }
};

TEST_F(BookmarkTest, NonEmptyBookmarkBarShownOnNTP) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  bookmarks::AddIfNotBookmarked(bookmark_model, GURL("https://www.test.com"),
                                std::u16string());

  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(BookmarkBar::SHOW,
            BookmarkBarController::From(browser())->bookmark_bar_state());
}

TEST_F(BookmarkTest, EmptyBookmarkBarNotShownOnNTP) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(BookmarkBar::HIDDEN,
            BookmarkBarController::From(browser())->bookmark_bar_state());
}

// Verify that the bookmark bar is hidden on custom NTP pages.
TEST_F(BookmarkTest, BookmarkBarOnCustomNTP) {
  // Create a empty commited web contents.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  web_contents->GetController().LoadURL(
      GURL(url::kAboutBlankURL), content::Referrer(), ui::PAGE_TRANSITION_LINK,
      std::string());

  // Give it a NTP virtual URL.
  content::NavigationController* controller = &web_contents->GetController();
  content::NavigationEntry* entry = controller->GetVisibleEntry();
  entry->SetVirtualURL(GURL(chrome::kChromeUINewTabURL));

  // Verify that the bookmark bar is hidden.
  EXPECT_EQ(BookmarkBar::HIDDEN,
            BookmarkBarController::From(browser())->bookmark_bar_state());
  browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  true);
  EXPECT_EQ(BookmarkBar::HIDDEN,
            BookmarkBarController::From(browser())->bookmark_bar_state());
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
  base::flat_map<int, bookmarks::TabGroupData> groups_by_index;
  for (int i = 0; i < 6; i++) {
    tab_entries.push_back(test_url);
    groups_by_index.emplace(i, std::make_pair(std::nullopt, u""));
  }

  bookmarks::GetURLsAndFoldersForTabEntries(&(details.bookmark_data.children),
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
  base::flat_map<int, bookmarks::TabGroupData> groups_by_index;
  for (int i = 0; i < 6; i++) {
    tab_entries.push_back(test_url);
    groups_by_index.emplace(i,
                            std::make_pair(std::make_optional(group_id), u""));
  }

  bookmarks::GetURLsAndFoldersForTabEntries(&(details.bookmark_data.children),
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
  base::flat_map<int, bookmarks::TabGroupData> groups_by_index;
  for (int i = 0; i < 6; i++) {
    tab_entries.push_back(test_url);
    groups_by_index.emplace(
        i, std::make_pair(
               std::make_optional(tab_groups::TabGroupId::GenerateNew()), u""));
  }

  bookmarks::GetURLsAndFoldersForTabEntries(&(details.bookmark_data.children),
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
  base::flat_map<int, bookmarks::TabGroupData> groups_by_index;
  for (int i = 0; i < 6; i++) {
    tab_entries.push_back(test_url);
    groups_by_index.emplace(
        i, std::make_pair(
               i >= 1 && i <= 3 ? std::make_optional(group_id) : std::nullopt,
               u""));
  }

  bookmarks::GetURLsAndFoldersForTabEntries(&(details.bookmark_data.children),
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
  base::flat_map<int, bookmarks::TabGroupData> groups_by_index;
  for (int i = 0; i < 6; i++) {
    tab_entries.push_back(test_url);
    groups_by_index.emplace(
        i, std::make_pair(
               i % 2 == 0
                   ? std::make_optional(tab_groups::TabGroupId::GenerateNew())
                   : std::nullopt,
               u""));
  }

  bookmarks::GetURLsAndFoldersForTabEntries(&(details.bookmark_data.children),
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

TEST_F(BookmarkTest, GetURLsAndFoldersForTabGroup) {
  // Deflake the test by setting TabGroupSyncService initialized.
  tab_groups::TabGroupSyncService* service =
      static_cast<tab_groups::TabGroupSyncService*>(
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(
              browser()->profile()));
  service->SetIsInitializedForTesting(true);
  const std::vector<GURL> urls = {GURL("http://localhost:8000/"),
                                  GURL("http://localhost:8001/"),
                                  GURL("http://localhost:8002/")};
  for (const auto& url : urls) {
    AddTab(browser(), url);
  }
  std::vector<int> tab_indices = {0, 1, 2};
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup(tab_indices);
  const TabGroup* tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);

  std::vector<BookmarkEditor::EditDetails::BookmarkData> folder_data;
  bookmarks::GetURLsAndFoldersForTabGroup(browser(), *tab_group, &folder_data);

  EXPECT_EQ(folder_data.size(), urls.size());
  for (size_t i = 0; i < urls.size(); ++i) {
    EXPECT_EQ(folder_data[urls.size() - 1 - i].url.value(), urls[i]);
  }
}

TEST_F(BookmarkTest, SuggestsUniqueTabGroupName) {
  auto service = std::make_unique<tab_groups::FakeTabGroupSyncService>();

  const std::u16string base_title = u"Test";

  // No existing groups, should return the original title.
  EXPECT_EQ(base_title,
            bookmarks::SuggestUniqueTabGroupName(base_title, service.get()));

  // Group with same name exists, should return "Test (1)".
  AddGroup(base_title, service.get());
  EXPECT_EQ(base_title + u" (1)",
            bookmarks::SuggestUniqueTabGroupName(base_title, service.get()));

  // "Test" and "Test (1)" exist, should return "Test (2)".
  AddGroup(base_title + u" (1)", service.get());
  EXPECT_EQ(base_title + u" (2)",
            bookmarks::SuggestUniqueTabGroupName(base_title, service.get()));

  // "Test", "Test (1)", "Test (3)" exist, should return "Test (2)".
  AddGroup(base_title + u" (3)", service.get());
  EXPECT_EQ(base_title + u" (2)",
            bookmarks::SuggestUniqueTabGroupName(base_title, service.get()));
}

TEST_F(BookmarkTest, SuggestsUniqueTabGroupNameReachesLimit) {
  auto service = std::make_unique<tab_groups::FakeTabGroupSyncService>();

  const std::u16string base_title = u"Test";
  AddGroup(base_title, service.get());
  for (int i = 1; i < 100; ++i) {
    AddGroup(base_title + u" (" + base::NumberToString16(i) + u")",
             service.get());
  }

  // All names from "Test" to "Test (99)" are taken. Should suggest "Test
  // (100)".
  EXPECT_EQ(base_title + u" (100)",
            bookmarks::SuggestUniqueTabGroupName(base_title, service.get()));

  // Add "Test (100)" as well. Should still suggest "Test (100)" as it's the
  // fallback.
  AddGroup(base_title + u" (100)", service.get());
  EXPECT_EQ(base_title + u" (100)",
            bookmarks::SuggestUniqueTabGroupName(base_title, service.get()));
}
