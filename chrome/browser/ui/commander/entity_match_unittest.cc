// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/entity_match.h"

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/commander/command_source.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/web_contents_tester.h"

namespace commander {

class CommanderEntityMatchTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    BrowserList::GetInstance()->SetLastActive(browser());
  }

  // Creates and returns a browser with `title` as its user title.
  // If `profile` is provided, it is used, otherwise uses the profile of this
  // test's browser.
  std::unique_ptr<Browser> CreateAndActivateBrowser(
      const std::string& title,
      Profile* browser_profile = nullptr) {
    Browser::CreateParams params(browser_profile ? browser_profile : profile(),
                                 true);
    auto browser = CreateBrowserWithTestWindowForParams(params);
    browser->SetWindowUserTitle(title);
    BrowserList::GetInstance()->SetLastActive(browser.get());
    return browser;
  }

  // Creates a tab per string in |titles|, then places each one in a group,
  // titled with the corresponding string.
  void CreateGroups(std::vector<std::u16string> titles) {
    // Create the tabs first so they don't get autogrouped and make odd things
    // happen.
    for (size_t i = 0; i < titles.size(); ++i)
      AddTab(browser(), GURL("chrome://newtab"));
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    TabGroupModel* group_model = tab_strip_model->group_model();
    for (size_t i = 0; i < titles.size(); ++i) {
      tab_groups::TabGroupId group =
          tab_strip_model->AddToNewGroup({static_cast<int>(i)});
      tab_groups::TabGroupVisualData data(
          titles.at(i), tab_groups::TabGroupColorId::kGrey, false);
      group_model->GetTabGroup(group)->SetVisualData(data);
    }
  }

  void CreateTabs(std::vector<std::u16string> titles) {
    for (const auto& title : titles) {
      GURL url("chrome://newtab");
      AddTab(browser(), url);
      NavigateAndCommitActiveTabWithTitle(browser(), url, title);
    }
  }
};

TEST_F(CommanderEntityMatchTest, WindowExcludesCurrentBrowser) {
  std::string title("Title");
  browser()->SetWindowUserTitle(title);
  auto other_browser = CreateAndActivateBrowser(title);

  auto matches =
      WindowsMatchingInput(browser(), base::UTF8ToUTF16(title), false);
  EXPECT_EQ(matches.size(), 1u);
}

TEST_F(CommanderEntityMatchTest, WindowIncludesAllProfilesIfUnrestricted) {
  std::string title("Title");

  auto same_profile_browser = CreateAndActivateBrowser(title);
  TestingProfile* other_profile =
      profile_manager()->CreateTestingProfile("other");
  auto other_profile_browser = CreateAndActivateBrowser(title, other_profile);

  auto matches =
      WindowsMatchingInput(browser(), base::UTF8ToUTF16(title), false);
  EXPECT_EQ(matches.size(), 2u);
}

TEST_F(CommanderEntityMatchTest, WindowOmitsNonmatchingProfilesIfRestricted) {
  std::string title("Title");
  auto same_profile_browser = CreateAndActivateBrowser(title);

  TestingProfile* other_profile =
      profile_manager()->CreateTestingProfile("other");
  auto other_profile_browser = CreateAndActivateBrowser(title, other_profile);

  auto matches =
      WindowsMatchingInput(browser(), base::UTF8ToUTF16(title), true);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches.at(0).browser, same_profile_browser.get());
}

TEST_F(CommanderEntityMatchTest, WindowOnlyIncludesMatches) {
  auto browser_with_match = CreateAndActivateBrowser("Orange juice");
  auto browser_without_match = CreateAndActivateBrowser("Aqua regia");

  auto matches = WindowsMatchingInput(browser(), u"orange", true);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches.at(0).browser, browser_with_match.get());
}

TEST_F(CommanderEntityMatchTest, WindowRanksMatches) {
  auto browser_best_match = CreateAndActivateBrowser("Orange juice");
  auto browser_good_match =
      CreateAndActivateBrowser("Oracular Nouns Gesture Electrically");

  auto matches = WindowsMatchingInput(browser(), u"orange", true);
  ASSERT_EQ(matches.size(), 2u);
  base::ranges::sort(matches, std::greater<>(), &WindowMatch::score);
  EXPECT_EQ(matches.at(0).browser, browser_best_match.get());
}

TEST_F(CommanderEntityMatchTest, WindowMRUOrderWithNoInput) {
  auto browser1 = CreateAndActivateBrowser("Beep");
  auto browser2 = CreateAndActivateBrowser("Boop");

  // Browser 2 was activated last, so we expect it to be the top match.
  auto matches = WindowsMatchingInput(browser(), u"", true);
  ASSERT_EQ(matches.size(), 2u);
  base::ranges::sort(matches, std::greater<>(), &WindowMatch::score);
  EXPECT_EQ(matches.at(0).browser, browser2.get());

  BrowserList::GetInstance()->SetLastActive(browser1.get());
  // Activating browser 1 should have brought it to the top.
  matches = WindowsMatchingInput(browser(), u"", true);
  ASSERT_EQ(matches.size(), 2u);
  base::ranges::sort(matches, std::greater<>(), &WindowMatch::score);
  EXPECT_EQ(matches.at(0).browser, browser1.get());
}

TEST_F(CommanderEntityMatchTest, GroupReturnsAllWithNoInput) {
  CreateGroups({u"Foo", u"Bar", u"Baz"});

  EXPECT_EQ(GroupsMatchingInput(browser(), u"").size(), 3u);
}

TEST_F(CommanderEntityMatchTest, GroupExcludeWithNoInput) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());
  CreateGroups({u"Foo", u"Bar", u"Baz"});

  auto second_group = browser()->tab_strip_model()->GetTabGroupForTab(1);
  EXPECT_TRUE(second_group.has_value());
  EXPECT_EQ(GroupsMatchingInput(browser(), u"", second_group).size(), 2u);
}

TEST_F(CommanderEntityMatchTest, GroupOnlyIncludesMatches) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());
  CreateGroups({u"Orange juice", u"Aqua Regia"});

  auto matches = GroupsMatchingInput(browser(), u"Orange");
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches.at(0).title, u"Orange juice");
}

TEST_F(CommanderEntityMatchTest, GroupRanksMatches) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());
  CreateGroups({u"Oracular Nouns Gesture Electrically", u"Orange juice"});

  auto matches = GroupsMatchingInput(browser(), u"orange");
  ASSERT_EQ(matches.size(), 2u);
  base::ranges::sort(matches, std::greater<>(), &GroupMatch::score);
  EXPECT_EQ(matches.at(0).title, u"Orange juice");
}

TEST_F(CommanderEntityMatchTest, GroupExcludeWithInput) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());
  CreateGroups({u"William of Orange", u"Orange juice"});

  auto first_group = browser()->tab_strip_model()->GetTabGroupForTab(0);
  auto matches = GroupsMatchingInput(browser(), u"orange", first_group);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches.at(0).title, u"Orange juice");
}

TEST_F(CommanderEntityMatchTest, TabReturnsAllWithNoInput) {
  CreateTabs({u"A", u"B", u"C"});

  EXPECT_EQ(TabsMatchingInput(browser(), u"").size(), 3u);
}

TEST_F(CommanderEntityMatchTest, TabOnlyPinnedExcludesUnpinned) {
  CreateTabs({u"A", u"B", u"C"});
  browser()->tab_strip_model()->SetTabPinned(1, true);

  TabSearchOptions options;
  options.only_pinned = true;
  auto matches = TabsMatchingInput(browser(), u"", options);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches.at(0).title, u"B");
}

TEST_F(CommanderEntityMatchTest, TabOnlyUnpinnedExcludesPinned) {
  CreateTabs({u"A", u"B", u"C"});
  browser()->tab_strip_model()->SetTabPinned(1, true);

  TabSearchOptions options;
  options.only_unpinned = true;
  auto matches = TabsMatchingInput(browser(), u"", options);
  ASSERT_EQ(matches.size(), 2u);
  EXPECT_EQ(base::ranges::find(matches, u"B", &TabMatch::title), matches.end());
}

TEST_F(CommanderEntityMatchTest, TabExcludeTabGroupExcludes) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());
  CreateTabs({u"A", u"B", u"C"});
  browser()->tab_strip_model()->AddToNewGroup({1});
  browser()->tab_strip_model()->AddToNewGroup({2});
  auto first_group = browser()->tab_strip_model()->GetTabGroupForTab(1);
  EXPECT_TRUE(first_group.has_value());

  TabSearchOptions options;
  options.exclude_tab_group = first_group;
  auto matches = TabsMatchingInput(browser(), u"", options);
  EXPECT_EQ(matches.size(), 2u);
  EXPECT_EQ(base::ranges::find(matches, u"B", &TabMatch::title), matches.end());
}

TEST_F(CommanderEntityMatchTest, TabOnlyTabGroupExcludesOthers) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());
  CreateTabs({u"A", u"B", u"C"});
  browser()->tab_strip_model()->AddToNewGroup({1});
  browser()->tab_strip_model()->AddToNewGroup({2});
  auto first_group = browser()->tab_strip_model()->GetTabGroupForTab(1);
  EXPECT_TRUE(first_group.has_value());

  TabSearchOptions options;
  options.only_tab_group = first_group;
  auto matches = TabsMatchingInput(browser(), u"", options);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches.at(0).title, u"B");
}

TEST_F(CommanderEntityMatchTest, TabOnlyAudibleExcludesOthers) {
  CreateTabs({u"A", u"B", u"C"});
  browser()->tab_strip_model()->InsertWebContentsAt(
      1, content::WebContentsTester::CreateTestWebContents(profile(), nullptr),
      AddTabTypes::ADD_NONE);
  content::WebContentsTester::For(
      browser()->tab_strip_model()->GetWebContentsAt(1))
      ->SetIsCurrentlyAudible(true);

  TabSearchOptions options;
  options.only_audible = true;
  auto matches = TabsMatchingInput(browser(), u"", options);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches.at(0).index, 1);
}

TEST_F(CommanderEntityMatchTest, TabOnlyMutedExcludesOthers) {
  CreateTabs({u"A", u"B", u"C"});
  browser()->tab_strip_model()->GetWebContentsAt(1)->SetAudioMuted(true);

  TabSearchOptions options;
  options.only_muted = true;
  auto matches = TabsMatchingInput(browser(), u"", options);
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches.at(0).index, 1);
}

TEST_F(CommanderEntityMatchTest, TabOnlyIncludesMatches) {
  CreateTabs({u"Orange juice", u"Aqua regia"});

  auto matches = TabsMatchingInput(browser(), u"orange");
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches.at(0).title, u"Orange juice");
}

TEST_F(CommanderEntityMatchTest, TabOnlyRanksMatches) {
  CreateTabs({u"Oracular Nouns Gesture Electrically", u"Orange juice"});

  auto matches = TabsMatchingInput(browser(), u"range");
  ASSERT_EQ(matches.size(), 2u);
  EXPECT_EQ(matches.at(0).title, u"Orange juice");

  base::ranges::sort(matches, std::greater<>(), &TabMatch::score);
  EXPECT_EQ(matches.at(0).title, u"Orange juice");
}

}  // namespace commander
