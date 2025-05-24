// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model.h"

#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/test/base/menu_model_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

// A TabStripModelDelegate for simulating web apps.
class WebAppTabStripModelDelegate : public TestTabStripModelDelegate {
 public:
  bool IsForWebApp() override { return true; }

  bool SupportsReadLater() override { return false; }
};

// A TabMenuModelDelegate that simulates no other browser windows being open.
class TabMenuModelTestDelegate : public TabMenuModelDelegate {
 public:
  std::vector<Browser*> GetOtherBrowserWindows(bool is_app) override {
    return {};
  }
};

class TabMenuModelTest : public MenuModelTest, public ::testing::Test {
 public:
  TabMenuModelTest() = default;
  ~TabMenuModelTest() override = default;

  Profile* profile() { return &profile_; }

  TabMenuModelDelegate& menu_model_delegate() { return menu_model_delegate_; }

 private:
  tabs::PreventTabFeatureInitialization prevent_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  TabMenuModelTestDelegate menu_model_delegate_;
};

TEST_F(TabMenuModelTest, TabbedWebApp) {
  // Create a tabbed web app window without home tab.
  WebAppTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());

  tab_strip_model.AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())),
      true);

  TabMenuModel model(&delegate_, &menu_model_delegate(), &tab_strip_model, 0);

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
  WebAppTabStripModelDelegate delegate;
  TabStripModel tab_strip_model(&delegate, profile());
  tab_strip_model.AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())),
      true);

  // Pin the first tab so we get the pinned home tab menu.
  tab_strip_model.SetTabPinned(0, true);

  TabMenuModel home_tab_model(&delegate_, &menu_model_delegate(),
                              &tab_strip_model, 0);

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

  tab_strip_model.AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())),
      true);
  EXPECT_EQ(tab_strip_model.count(), 2);
  EXPECT_FALSE(tab_strip_model.IsTabSelected(0));
  EXPECT_TRUE(tab_strip_model.IsTabSelected(1));

  TabMenuModel regular_tab_model(&delegate_, &menu_model_delegate(),
                                 &tab_strip_model, 1);

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
