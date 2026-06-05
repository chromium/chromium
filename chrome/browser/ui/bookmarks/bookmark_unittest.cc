// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_bar_visibility_state.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#include "components/search/ntp_features.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"

class BookmarkTest : public ChromeRenderViewHostTestHarness {
 public:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory()}};
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Bind Mock Browser to return test profile and a real user data host.
    ON_CALL(mock_browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile()));
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(user_data_host_));

    // Initialize BrowserWindowFullscreenController on our user data host,
    // which is required by BookmarkBarController.
    fullscreen_controller_ =
        std::make_unique<BrowserWindowFullscreenController>(
            mock_browser_window_interface_);

    // Set up TabStripModel and delegate.
    tab_strip_model_delegate_.SetBrowserWindowInterface(
        &mock_browser_window_interface_);
    tab_strip_model_ =
        std::make_unique<TabStripModel>(&tab_strip_model_delegate_, profile());

    browser_actions_ =
        std::make_unique<BrowserActions>(&mock_browser_window_interface_);

    user_education_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserUserEducationInterface>>(
            &mock_browser_window_interface_);

    ON_CALL(mock_browser_window_interface_, GetTabStripModel())
        .WillByDefault(testing::Return(tab_strip_model_.get()));
    ON_CALL(mock_browser_window_interface_, GetActions())
        .WillByDefault(testing::Return(browser_actions_.get()));
    ON_CALL(mock_browser_window_interface_, GetFeatures())
        .WillByDefault(testing::ReturnRef(features_));
  }

  void TearDown() override {
    user_education_interface_.reset();
    browser_actions_.reset();
    tab_strip_model_.reset();
    fullscreen_controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void AddGroup(const std::u16string& title,
                tab_groups::TabGroupSyncService* service) {
    tab_groups::SavedTabGroup group(
        title, tab_groups::TabGroupColorId::kGrey, {}, std::nullopt,
        base::Uuid::GenerateRandomV4(), std::nullopt);
    service->AddGroup(std::move(group));
  }

 protected:
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<BrowserWindowFullscreenController> fullscreen_controller_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<BrowserActions> browser_actions_;
  std::unique_ptr<MockBrowserUserEducationInterface> user_education_interface_;
  BrowserWindowFeatures features_;
};

TEST_F(BookmarkTest, NonEmptyBookmarkBarShownOnNTP) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  bookmarks::AddIfNotBookmarked(bookmark_model, GURL("https://www.test.com"),
                                std::u16string());

  BookmarkBarController controller(mock_browser_window_interface_,
                                   *tab_strip_model_);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  tab_strip_model_->AppendWebContents(std::move(web_contents),
                                      /*foreground=*/true);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      tab_strip_model_->GetActiveWebContents(),
      chrome::ChromeUINewTabURLAsGURL());

  EXPECT_EQ(BookmarkBar::SHOW, controller.bookmark_bar_state());
}

TEST_F(BookmarkTest, EmptyBookmarkBarNotShownOnNTP) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  BookmarkBarController controller(mock_browser_window_interface_,
                                   *tab_strip_model_);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  tab_strip_model_->AppendWebContents(std::move(web_contents),
                                      /*foreground=*/true);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      tab_strip_model_->GetActiveWebContents(),
      chrome::ChromeUINewTabURLAsGURL());

  EXPECT_EQ(BookmarkBar::HIDDEN, controller.bookmark_bar_state());
}

TEST_F(BookmarkTest, BookmarkBarOnCustomNTP) {
  BookmarkBarController controller(mock_browser_window_interface_,
                                   *tab_strip_model_);

  // Create a empty committed web contents.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL(url::kAboutBlankURL));

  // Give it a NTP virtual URL.
  content::NavigationController* nav_controller =
      &web_contents->GetController();
  content::NavigationEntry* entry = nav_controller->GetVisibleEntry();
  entry->SetVirtualURL(chrome::ChromeUINewTabURLAsGURL());

  // Verify that the bookmark bar is hidden.
  EXPECT_EQ(BookmarkBar::HIDDEN, controller.bookmark_bar_state());

  tab_strip_model_->AppendWebContents(std::move(web_contents),
                                      /*foreground=*/true);
  EXPECT_EQ(BookmarkBar::HIDDEN, controller.bookmark_bar_state());
}

TEST_F(BookmarkTest, BookmarkReaderModePageActuallyBookmarksOriginal) {
  GURL original("https://www.example.com/article.html");
  GURL distilled = dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      dom_distiller::kDomDistillerScheme, original, "Article title");

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents.get(),
                                                             distilled);

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
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile()));
  if (service) {
    service->SetIsInitializedForTesting(true);
  }
  const std::vector<GURL> urls = {GURL("http://localhost:8000/"),
                                  GURL("http://localhost:8001/"),
                                  GURL("http://localhost:8002/")};
  for (const auto& url : urls) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents.get(), url);
    tab_strip_model_->AppendWebContents(std::move(web_contents),
                                        /*foreground=*/true);
  }
  std::vector<int> tab_indices = {0, 1, 2};
  tab_groups::TabGroupId group_id =
      tab_strip_model_->AddToNewGroup(tab_indices);
  const TabGroup* tab_group =
      tab_strip_model_->group_model()->GetTabGroup(group_id);

  std::vector<BookmarkEditor::EditDetails::BookmarkData> folder_data;
  bookmarks::GetURLsAndFoldersForTabGroup(tab_strip_model_.get(), *tab_group,
                                          &folder_data);

  EXPECT_EQ(folder_data.size(), urls.size());
  for (size_t i = 0; i < urls.size(); ++i) {
    EXPECT_EQ(folder_data[i].url.value(), urls[i]);
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

// Ensures that when the bookmark bar is enabled
// `bookmarks::prefs::kBookmarkBarVisibilityState` is updated accordingly.
TEST_F(BookmarkTest, NtpSimplificationVisibilityPrefUpdated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ntp_features::kNtpSimplificationBookmarkBar);

  profile()->GetPrefs()->SetBoolean(bookmarks::prefs::kShowBookmarkBar, true);

  // Verify that the pref is initially at its default value.
  EXPECT_EQ(
      profile()->GetPrefs()->GetInteger(
          bookmarks::prefs::kBookmarkBarVisibilityState),
      static_cast<int>(bookmarks::BookmarkBarVisibilityState::kOnlyShowOnNtp));

  BookmarkBarController controller(mock_browser_window_interface_,
                                   *tab_strip_model_);

  // Verify that the pref was updated.
  EXPECT_EQ(
      profile()->GetPrefs()->GetInteger(
          bookmarks::prefs::kBookmarkBarVisibilityState),
      static_cast<int>(bookmarks::BookmarkBarVisibilityState::kAlwaysShow));
}

// Ensures that when `kBookmarkBarVisibilityState` is set to `kAlwaysHide`, the
// bookmarks bar successfully hides on the New Tab Page (NTP).
TEST_F(BookmarkTest, NtpSimplificationAlwaysHideWorksOnNTP) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ntp_features::kNtpSimplificationBookmarkBar);

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
  bookmarks::AddIfNotBookmarked(bookmark_model, GURL("https://www.test.com"),
                                std::u16string());

  profile()->GetPrefs()->SetInteger(
      bookmarks::prefs::kBookmarkBarVisibilityState,
      static_cast<int>(bookmarks::BookmarkBarVisibilityState::kAlwaysHide));

  BookmarkBarController controller(mock_browser_window_interface_,
                                   *tab_strip_model_);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  tab_strip_model_->AppendWebContents(std::move(web_contents),
                                      /*foreground=*/true);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      tab_strip_model_->GetActiveWebContents(),
      chrome::ChromeUINewTabURLAsGURL());

  EXPECT_EQ(BookmarkBar::HIDDEN, controller.bookmark_bar_state());
}

// Ensures that when `kBookmarkBarVisibilityState` changes while the bookmark
// bar simplification feature is enabled, `kShowBookmarkBar` is kept
// synchronized.
TEST_F(BookmarkTest, NtpSimplificationVisibilityPrefSyncsShowBookmarkBar) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ntp_features::kNtpSimplificationBookmarkBar);

  profile()->GetPrefs()->SetBoolean(bookmarks::prefs::kShowBookmarkBar, false);

  BookmarkBarController controller(mock_browser_window_interface_,
                                   *tab_strip_model_);

  // Transition to kAlwaysShow should sync kShowBookmarkBar to true.
  profile()->GetPrefs()->SetInteger(
      bookmarks::prefs::kBookmarkBarVisibilityState,
      static_cast<int>(bookmarks::BookmarkBarVisibilityState::kAlwaysShow));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(bookmarks::prefs::kShowBookmarkBar));

  // Transition to kAlwaysHide should sync kShowBookmarkBar to false.
  profile()->GetPrefs()->SetInteger(
      bookmarks::prefs::kBookmarkBarVisibilityState,
      static_cast<int>(bookmarks::BookmarkBarVisibilityState::kAlwaysHide));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(bookmarks::prefs::kShowBookmarkBar));
}

class BookmarkBarTabGroupsTest : public BookmarkTest {
 public:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    auto factories = BookmarkTest::GetTestingFactories();
    factories.push_back(TestingProfile::TestingFactory{
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<tab_groups::FakeTabGroupSyncService>();
        })});
    return factories;
  }
};

TEST_F(BookmarkBarTabGroupsTest, SavedTabGroupsRespectPrefOnNTP) {
  // Ensure bookmark model is loaded (empty).
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
  ASSERT_FALSE(bookmark_model->HasBookmarks());

  // Get the fake tab group sync service.
  auto* service = static_cast<tab_groups::FakeTabGroupSyncService*>(
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile()));
  ASSERT_TRUE(service);

  // Add a saved tab group.
  AddGroup(u"Test Group", service);
  ASSERT_FALSE(service->GetAllGroups().empty());

  BookmarkBarController controller(mock_browser_window_interface_,
                                   *tab_strip_model_);

  // Set NTP as active tab.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  tab_strip_model_->AppendWebContents(std::move(web_contents),
                                      /*foreground=*/true);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      tab_strip_model_->GetActiveWebContents(),
      chrome::ChromeUINewTabURLAsGURL());

  // Case 1: Pref is ON (default is usually ON, but let's set it explicitly).
  profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar, true);
  EXPECT_EQ(BookmarkBar::SHOW, controller.bookmark_bar_state());

  // Case 2: Pref is OFF.
  profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar, false);
  EXPECT_EQ(BookmarkBar::HIDDEN, controller.bookmark_bar_state());
}
