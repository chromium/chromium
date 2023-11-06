// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feed/web_feed_tab_helper.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/menu_model_test.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_features.h"

class TabMenuModelTest : public MenuModelTest,
                         public BrowserWithTestWindowTest {
};

TEST_F(TabMenuModelTest, Basics) {
  chrome::NewTab(browser());
  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);

  // Verify it has items. The number varies by platform, so we don't check
  // the exact number.
  EXPECT_GT(model.GetItemCount(), 5u);

  int item_count = 0;
  CountEnabledExecutable(&model, &item_count);
  EXPECT_GT(item_count, 0);
  EXPECT_EQ(item_count, delegate_.execute_count_);
  EXPECT_EQ(item_count, delegate_.enable_count_);
}

TEST_F(TabMenuModelTest, OrganizeTabs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kTabOrganization, features::kChromeRefresh2023}, {});
  chrome::NewTab(browser());
  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);

  // Verify that CommandOrganizeTabs is in the menu.
  EXPECT_TRUE(model.GetIndexOfCommandId(TabStripModel::CommandOrganizeTabs)
                  .has_value());
}

TEST_F(TabMenuModelTest, MoveToNewWindow) {
  chrome::NewTab(browser());
  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(),
                     browser()->tab_strip_model(), 0);

  // Verify that CommandMoveTabsToNewWindow is in the menu.
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandMoveTabsToNewWindow)
          .has_value());
}

TEST_F(TabMenuModelTest, AddToExistingGroupSubmenu) {
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  tab_strip_model->AddToNewGroup({0});
  tab_strip_model->AddToNewGroup({1});
  tab_strip_model->AddToNewGroup({2});

  TabMenuModel menu(&delegate_, browser()->tab_menu_model_delegate(),
                    tab_strip_model, 3);

  size_t submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandAddToExistingGroup)
          .value();
  ui::MenuModel* submenu = menu.GetSubmenuModelAt(submenu_index);

  EXPECT_EQ(submenu->GetItemCount(), 5u);
  EXPECT_EQ(submenu->GetCommandIdAt(0),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId);
  EXPECT_EQ(submenu->GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(submenu->GetCommandIdAt(2),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 1);
  EXPECT_FALSE(submenu->GetIconAt(2).IsEmpty());
  EXPECT_EQ(submenu->GetCommandIdAt(3),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 2);
  EXPECT_EQ(submenu->GetCommandIdAt(4),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 3);
}

TEST_F(TabMenuModelTest, AddToExistingGroupSubmenu_DoesNotIncludeCurrentGroup) {
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  tab_strip_model->AddToNewGroup({0});
  tab_strip_model->AddToNewGroup({1});
  tab_strip_model->AddToNewGroup({2});

  TabMenuModel menu(&delegate_, browser()->tab_menu_model_delegate(),
                    tab_strip_model, 1);

  size_t submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandAddToExistingGroup)
          .value();
  ui::MenuModel* submenu = menu.GetSubmenuModelAt(submenu_index);

  EXPECT_EQ(submenu->GetItemCount(), 4u);
  EXPECT_EQ(submenu->GetCommandIdAt(0),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId);
  EXPECT_EQ(submenu->GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(submenu->GetCommandIdAt(2),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 1);
  EXPECT_FALSE(submenu->GetIconAt(2).IsEmpty());
  EXPECT_EQ(submenu->GetCommandIdAt(3),
            ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId + 2);
}

// In some cases, groups may change after the menu is created. For example an
// extension may modify groups while the menu is open. If a group referenced in
// the menu goes away, ensure we handle this gracefully.
//
// Regression test for crbug.com/1197875
TEST_F(TabMenuModelTest, AddToExistingGroupAfterGroupDestroyed) {
  chrome::NewTab(browser());
  chrome::NewTab(browser());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->AddToNewGroup({0});

  TabMenuModel menu(&delegate_, browser()->tab_menu_model_delegate(),
                    tab_strip_model, 1);

  size_t submenu_index =
      menu.GetIndexOfCommandId(TabStripModel::CommandAddToExistingGroup)
          .value();
  ui::MenuModel* submenu = menu.GetSubmenuModelAt(submenu_index);

  EXPECT_EQ(submenu->GetItemCount(), 3u);

  // Ungroup the tab at 0 to make the group in the menu dangle.
  tab_strip_model->RemoveFromGroup({0});

  // Try adding to the group from the menu.
  submenu->ActivatedAt(2);

  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(0).has_value());
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(1).has_value());
}

TEST_F(TabMenuModelTest, FollowOrUnfollow) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(feed::kWebUiFeed);

  chrome::NewTab(browser());
  chrome::NewTab(browser());
  chrome::NewTab(browser());

  TabStripModel* tab_strip = browser()->tab_strip_model();

  content::WebContents* web_contents0 = tab_strip->GetWebContentsAt(0);
  feed::WebFeedTabHelper::CreateForWebContents(web_contents0);
  feed::WebFeedTabHelper* web_feed_tab_helper0 =
      feed::WebFeedTabHelper::FromWebContents(web_contents0);

  content::WebContents* web_contents1 = tab_strip->GetWebContentsAt(1);
  feed::WebFeedTabHelper::CreateForWebContents(web_contents1);
  feed::WebFeedTabHelper* web_feed_tab_helper1 =
      feed::WebFeedTabHelper::FromWebContents(web_contents1);

  content::WebContents* web_contents2 = tab_strip->GetWebContentsAt(2);
  feed::WebFeedTabHelper::CreateForWebContents(web_contents2);
  feed::WebFeedTabHelper* web_feed_tab_helper2 =
      feed::WebFeedTabHelper::FromWebContents(web_contents2);

  tab_strip->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  tab_strip->ExtendSelectionTo(2);

  // Neither "Follow site" nor "Unfollow site" should be added when there is at
  // least one site in kUnknown state.
  {
    web_feed_tab_helper0->SetWebFeedInfoForTesting(
        web_contents0->GetLastCommittedURL(), TabWebFeedFollowState::kFollowed,
        std::string());
    web_feed_tab_helper1->SetWebFeedInfoForTesting(
        web_contents1->GetLastCommittedURL(),
        TabWebFeedFollowState::kNotFollowed, std::string());
    web_feed_tab_helper2->SetWebFeedInfoForTesting(
        web_contents2->GetLastCommittedURL(), TabWebFeedFollowState::kUnknown,
        std::string());
    TabMenuModel menu(&delegate_, browser()->tab_menu_model_delegate(),
                      tab_strip, 0);
    EXPECT_FALSE(
        menu.GetIndexOfCommandId(TabStripModel::CommandFollowSite).has_value());
    EXPECT_FALSE(menu.GetIndexOfCommandId(TabStripModel::CommandUnfollowSite)
                     .has_value());
  }

  // "Unfollow site" should be added when all sites are in kFollowed state.
  {
    web_feed_tab_helper0->SetWebFeedInfoForTesting(
        web_contents0->GetLastCommittedURL(), TabWebFeedFollowState::kFollowed,
        std::string());
    web_feed_tab_helper1->SetWebFeedInfoForTesting(
        web_contents1->GetLastCommittedURL(), TabWebFeedFollowState::kFollowed,
        std::string());
    web_feed_tab_helper2->SetWebFeedInfoForTesting(
        web_contents2->GetLastCommittedURL(), TabWebFeedFollowState::kFollowed,
        std::string());
    TabMenuModel menu(&delegate_, browser()->tab_menu_model_delegate(),
                      tab_strip, 0);
    EXPECT_FALSE(
        menu.GetIndexOfCommandId(TabStripModel::CommandFollowSite).has_value());
    EXPECT_TRUE(menu.GetIndexOfCommandId(TabStripModel::CommandUnfollowSite)
                    .has_value());
  }

  // "Follow site" should be added when not all sites are in kFollowed state.
  {
    web_feed_tab_helper0->SetWebFeedInfoForTesting(
        web_contents0->GetLastCommittedURL(), TabWebFeedFollowState::kFollowed,
        std::string());
    web_feed_tab_helper1->SetWebFeedInfoForTesting(
        web_contents1->GetLastCommittedURL(), TabWebFeedFollowState::kFollowed,
        std::string());
    web_feed_tab_helper2->SetWebFeedInfoForTesting(
        web_contents2->GetLastCommittedURL(),
        TabWebFeedFollowState::kNotFollowed, std::string());
    TabMenuModel menu(&delegate_, browser()->tab_menu_model_delegate(),
                      tab_strip, 0);
    EXPECT_TRUE(
        menu.GetIndexOfCommandId(TabStripModel::CommandFollowSite).has_value());
    EXPECT_FALSE(menu.GetIndexOfCommandId(TabStripModel::CommandUnfollowSite)
                     .has_value());
  }

  // Neither "Follow site" nor "Unfollow site" should be added when the recorded
  // URL in WebFeedTabHelper does not match with the last committed URL in
  // web contents.
  {
    web_feed_tab_helper0->SetWebFeedInfoForTesting(
        GURL("http://example.org/test"), TabWebFeedFollowState::kFollowed,
        std::string());
    web_feed_tab_helper1->SetWebFeedInfoForTesting(
        web_contents1->GetLastCommittedURL(), TabWebFeedFollowState::kFollowed,
        std::string());
    web_feed_tab_helper2->SetWebFeedInfoForTesting(
        web_contents2->GetLastCommittedURL(), TabWebFeedFollowState::kFollowed,
        std::string());
    TabMenuModel menu(&delegate_, browser()->tab_menu_model_delegate(),
                      tab_strip, 0);
    EXPECT_FALSE(
        menu.GetIndexOfCommandId(TabStripModel::CommandFollowSite).has_value());
    EXPECT_FALSE(menu.GetIndexOfCommandId(TabStripModel::CommandUnfollowSite)
                     .has_value());
  }
}

class TabMenuModelTestTabStripModelDelegate : public TestTabStripModelDelegate {
 public:
  bool IsForWebApp() override { return true; }

  bool SupportsReadLater() override { return false; }
};

TEST_F(TabMenuModelTest, TabbedWebApp) {
  // Create a tabbed web app window without home tab
  TabMenuModelTestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  strip.AppendWebContents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr),
      true);

  TabMenuModel model(&delegate_, browser()->tab_menu_model_delegate(), &strip,
                     0);

  // When adding/removing a menu item, either update this count and add it to
  // the list below or disable it for tabbed web apps.
  EXPECT_EQ(model.GetItemCount(), 7u);

  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandCopyURL).has_value());
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandReload).has_value());
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandGoBack).has_value());
  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandMoveTabsToNewWindow)
          .has_value());

  EXPECT_EQ(model.GetTypeAt(4), ui::MenuModel::TYPE_SEPARATOR);

  EXPECT_TRUE(
      model.GetIndexOfCommandId(TabStripModel::CommandCloseTab).has_value());
  EXPECT_TRUE(model.GetIndexOfCommandId(TabStripModel::CommandCloseOtherTabs)
                  .has_value());
}

TEST_F(TabMenuModelTest, TabbedWebAppHomeTab) {
  TabMenuModelTestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  strip.AppendWebContents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr),
      true);
  // Pin the first tab so we get the pinned home tab menu.
  strip.SetTabPinned(0, true);

  TabMenuModel home_tab_model(&delegate_, browser()->tab_menu_model_delegate(),
                              &strip, 0);

  // When adding/removing a menu item, either update this count and add it to
  // the list below or disable it for tabbed web apps.
  EXPECT_EQ(home_tab_model.GetItemCount(), 5u);

  EXPECT_TRUE(home_tab_model.GetIndexOfCommandId(TabStripModel::CommandCopyURL)
                  .has_value());
  EXPECT_TRUE(home_tab_model.GetIndexOfCommandId(TabStripModel::CommandReload)
                  .has_value());
  EXPECT_TRUE(home_tab_model.GetIndexOfCommandId(TabStripModel::CommandGoBack)
                  .has_value());

  EXPECT_EQ(home_tab_model.GetTypeAt(3), ui::MenuModel::TYPE_SEPARATOR);

  EXPECT_TRUE(
      home_tab_model.GetIndexOfCommandId(TabStripModel::CommandCloseAllTabs)
          .has_value());

  strip.AppendWebContents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr),
      true);
  EXPECT_EQ(strip.count(), 2);
  EXPECT_FALSE(strip.IsTabSelected(0));
  EXPECT_TRUE(strip.IsTabSelected(1));

  TabMenuModel regular_tab_model(
      &delegate_, browser()->tab_menu_model_delegate(), &strip, 1);

  // When adding/removing a menu item, either update this count and add it to
  // the list below or disable it for tabbed web apps.
  EXPECT_EQ(regular_tab_model.GetItemCount(), 8u);

  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandCopyURL)
          .has_value());
  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandReload)
          .has_value());
  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandGoBack)
          .has_value());
  EXPECT_TRUE(
      regular_tab_model
          .GetIndexOfCommandId(TabStripModel::CommandMoveTabsToNewWindow)
          .has_value());

  EXPECT_EQ(regular_tab_model.GetTypeAt(4), ui::MenuModel::TYPE_SEPARATOR);

  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandCloseTab)
          .has_value());
  EXPECT_TRUE(regular_tab_model
                  .GetIndexOfCommandId(TabStripModel::CommandCloseOtherTabs)
                  .has_value());
  EXPECT_TRUE(
      regular_tab_model.GetIndexOfCommandId(TabStripModel::CommandCloseAllTabs)
          .has_value());
}
