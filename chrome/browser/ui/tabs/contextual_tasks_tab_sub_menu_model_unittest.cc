// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/contextual_tasks_tab_sub_menu_model.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_controller.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

namespace contextual_tasks {
namespace {

using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAre;

class ContextualTasksTabSubMenuModelTest : public testing::Test {
 public:
  ContextualTasksTabSubMenuModelTest() {
    feature_list_.InitWithFeatures({kContextualTasksSidePanelRearchitecture,
                                    kContextualTasksForceEntryPointEligibility},
                                   {});
  }

  void SetUp() override {
    ON_CALL(mock_browser_window_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));
    delegate_.SetBrowserWindowInterface(&mock_browser_window_);
    mock_panel_controller_ =
        std::make_unique<NiceMock<MockContextualTasksPanelController>>(
            &mock_browser_window_);
  }

  tabs::TabInterface* AppendTabWithUrl(const GURL& url) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    content::WebContentsTester::For(web_contents.get())->NavigateAndCommit(url);
    tab_strip_model_.AppendWebContents(std::move(web_contents),
                                       /*foreground=*/true);
    return tab_strip_model_.GetTabAtIndex(tab_strip_model_.count() - 1);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  NiceMock<MockBrowserWindowInterface> mock_browser_window_;
  TestTabStripModelDelegate delegate_;
  TabStripModel tab_strip_model_{&delegate_, &profile_};
  std::unique_ptr<NiceMock<MockContextualTasksPanelController>>
      mock_panel_controller_;
};

TEST_F(ContextualTasksTabSubMenuModelTest,
       BuildsStartNewThreadItemAndExecutesSingleTab) {
  tabs::TabInterface* tab = AppendTabWithUrl(GURL("https://example.com"));

  ContextualTasksTabSubMenuModel submenu(&tab_strip_model_, 0);
  ASSERT_EQ(submenu.GetItemCount(), 1u);
  EXPECT_EQ(submenu.GetCommandIdAt(0),
            TabStripModel::CommandAiModeCreateNewThread);
  EXPECT_EQ(submenu.GetLabelAt(0),
            l10n_util::GetStringUTF16(IDS_TAB_CXMENU_AI_MODE_START_NEW_THREAD));
  EXPECT_TRUE(submenu.IsEnabledAt(0));
  EXPECT_FALSE(submenu.IsItemCheckedAt(0));

  EXPECT_CALL(*mock_panel_controller_,
              ShareTabsWithActiveOrNewThread(ElementsAre(tab->GetHandle())))
      .Times(1);
  submenu.ExecuteCommand(TabStripModel::CommandAiModeCreateNewThread, 0);
}

TEST_F(ContextualTasksTabSubMenuModelTest,
       ExecuteCommand_MultiTabSelectionFiltersInvalidUrls) {
  tabs::TabInterface* valid_tab_1 =
      AppendTabWithUrl(GURL("https://example.com/1"));
  AppendTabWithUrl(GURL("chrome://version"));
  tabs::TabInterface* valid_tab_2 =
      AppendTabWithUrl(GURL("https://example.com/2"));

  tab_strip_model_.SelectTabAt(0);
  tab_strip_model_.SelectTabAt(1);
  ASSERT_TRUE(tab_strip_model_.IsTabSelected(0));
  ASSERT_TRUE(tab_strip_model_.IsTabSelected(1));
  ASSERT_TRUE(tab_strip_model_.IsTabSelected(2));

  ContextualTasksTabSubMenuModel submenu(&tab_strip_model_, 0);
  EXPECT_CALL(*mock_panel_controller_,
              ShareTabsWithActiveOrNewThread(UnorderedElementsAre(
                  valid_tab_1->GetHandle(), valid_tab_2->GetHandle())))
      .Times(1);
  submenu.ExecuteCommand(TabStripModel::CommandAiModeCreateNewThread, 0);
}

TEST_F(ContextualTasksTabSubMenuModelTest,
       ExecuteCommand_UnselectedContextTabSharesOnlyContextTab) {
  tabs::TabInterface* unselected_tab =
      AppendTabWithUrl(GURL("https://example.com/unselected"));
  AppendTabWithUrl(GURL("https://example.com/selected"));
  ASSERT_FALSE(tab_strip_model_.IsTabSelected(0));
  ASSERT_TRUE(tab_strip_model_.IsTabSelected(1));

  ContextualTasksTabSubMenuModel submenu(&tab_strip_model_, 0);
  EXPECT_CALL(
      *mock_panel_controller_,
      ShareTabsWithActiveOrNewThread(ElementsAre(unselected_tab->GetHandle())))
      .Times(1);
  submenu.ExecuteCommand(TabStripModel::CommandAiModeCreateNewThread, 0);
}

TEST_F(ContextualTasksTabSubMenuModelTest,
       ExecuteCommand_ClosedContextTabNoOps) {
  AppendTabWithUrl(GURL("https://example.com"));
  ContextualTasksTabSubMenuModel submenu(&tab_strip_model_, 0);
  tab_strip_model_.CloseAllTabs();

  EXPECT_CALL(*mock_panel_controller_,
              ShareTabsWithActiveOrNewThread(testing::_))
      .Times(0);
  submenu.ExecuteCommand(TabStripModel::CommandAiModeCreateNewThread, 0);
}

TEST_F(ContextualTasksTabSubMenuModelTest,
       IsDisabledWhenAllSelectedUrlsAreInvalid) {
  AppendTabWithUrl(GURL("chrome://version"));

  ContextualTasksTabSubMenuModel submenu(&tab_strip_model_, 0);
  ASSERT_EQ(submenu.GetItemCount(), 1u);
  EXPECT_FALSE(submenu.IsEnabledAt(0));
}

TEST_F(ContextualTasksTabSubMenuModelTest,
       IsDisabledWhenContextualTasksFeatureDisabled) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(
      kContextualTasksSidePanelRearchitecture);

  AppendTabWithUrl(GURL("https://example.com"));

  ContextualTasksTabSubMenuModel submenu(&tab_strip_model_, 0);
  ASSERT_EQ(submenu.GetItemCount(), 1u);
  EXPECT_FALSE(submenu.IsEnabledAt(0));
}

TEST_F(ContextualTasksTabSubMenuModelTest,
       PopulatesRecentThreadsAndExecutesExistingThread) {
  tabs::TabInterface* tab = AppendTabWithUrl(GURL("https://example.com"));

  base::Uuid thread_1_id = base::Uuid::GenerateRandomV4();
  base::Uuid thread_2_id = base::Uuid::GenerateRandomV4();
  std::vector<ContextualTasksPanelController::RecentThreadInfo> recent_threads =
      {
          {thread_1_id, u"Recent Thread 1"},
          {thread_2_id, u"Recent Thread 2"},
      };
  ON_CALL(*mock_panel_controller_, GetRecentThreads())
      .WillByDefault(testing::Return(recent_threads));

  ContextualTasksTabSubMenuModel submenu(&tab_strip_model_, 0);
  ASSERT_EQ(submenu.GetItemCount(), 4u);
  EXPECT_EQ(submenu.GetCommandIdAt(0),
            TabStripModel::CommandAiModeCreateNewThread);
  EXPECT_EQ(submenu.GetTypeAt(1), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(submenu.GetCommandIdAt(2),
            ContextualTasksTabSubMenuModel::kMinRecentThreadCommandId);
  EXPECT_EQ(submenu.GetLabelAt(2), u"Recent Thread 1");
  EXPECT_TRUE(submenu.IsEnabledAt(2));
  EXPECT_EQ(submenu.GetCommandIdAt(3),
            ContextualTasksTabSubMenuModel::kMinRecentThreadCommandId + 1);
  EXPECT_EQ(submenu.GetLabelAt(3), u"Recent Thread 2");
  EXPECT_TRUE(submenu.IsEnabledAt(3));
  EXPECT_FALSE(submenu.IsCommandIdEnabled(
      ContextualTasksTabSubMenuModel::kMaxRecentThreadCommandId));

  EXPECT_CALL(*mock_panel_controller_,
              ShareTabsWithExistingThread(
                  thread_2_id, ElementsAre(tab->GetHandle()),
                  omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT))
      .Times(1);
  submenu.ExecuteCommand(
      ContextualTasksTabSubMenuModel::kMinRecentThreadCommandId + 1, 0);

  // Navigating the context tab to an internal URL disables recent thread items
  // while `ContainsIndex(context_index_)` is still true.
  content::WebContentsTester::For(tab->GetContents())
      ->NavigateAndCommit(GURL("chrome://version"));
  EXPECT_FALSE(submenu.IsEnabledAt(0));
  EXPECT_FALSE(submenu.IsEnabledAt(2));

  tab_strip_model_.CloseAllTabs();
  EXPECT_FALSE(submenu.IsEnabledAt(0));
  EXPECT_FALSE(submenu.IsEnabledAt(2));
}

}  // namespace
}  // namespace contextual_tasks
