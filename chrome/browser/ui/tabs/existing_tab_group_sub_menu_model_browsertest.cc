// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_tab_group_sub_menu_model.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tab_menu_model_delegate.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_on_close_helper.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "url/gurl.h"

namespace {
std::unique_ptr<TabMenuModelDelegate> CreateTabMenuModelDelegate(
    Browser* browser) {
  tab_groups::TabGroupSyncService* tgss =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser->profile());
  return std::make_unique<chrome::BrowserTabMenuModelDelegate>(
      browser->session_id(), browser->profile(), browser->app_controller(),
      tgss);
}

}  // namespace

class ExistingTabGroupSubMenuModelTest : public InProcessBrowserTest {
 public:
  ExistingTabGroupSubMenuModelTest() = default;
};

// Ensure that add to group submenu only appears when there is another group to
// move the tab into.
IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelTest, ShouldShowSubmenu) {
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);

  TabStripModel* model = browser()->tab_strip_model();
  ASSERT_EQ(model->group_model()->ListTabGroups().size(), 0U);
  model->AddToNewGroup({0});
  ASSERT_EQ(model->group_model()->ListTabGroups().size(), 1U);
  ASSERT_TRUE(model->GetTabGroupForTab(0).has_value());
  ASSERT_FALSE(model->GetTabGroupForTab(1).has_value());
  ASSERT_EQ(model->count(), 2);

  EXPECT_FALSE(
      ExistingTabGroupSubMenuModel::ShouldShowSubmenu(model, 0, nullptr));
  EXPECT_TRUE(
      ExistingTabGroupSubMenuModel::ShouldShowSubmenu(model, 1, nullptr));
}

// Validate that the submenu has the correct items.
IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelTest, BuildSubmenuItems) {
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);

  TabStripModel* model = browser()->tab_strip_model();
  model->AddToNewGroup({0});
  model->AddToNewGroup({1});
  ASSERT_EQ(model->group_model()->ListTabGroups().size(), 2U);
  ASSERT_TRUE(model->GetTabGroupForTab(0).has_value());
  ASSERT_TRUE(model->GetTabGroupForTab(1).has_value());
  ASSERT_FALSE(model->GetTabGroupForTab(2).has_value());
  ASSERT_EQ(model->count(), 3);

  ExistingTabGroupSubMenuModel menu1(nullptr, nullptr, model, 0);
  EXPECT_EQ(3u, menu1.GetItemCount());

  ExistingTabGroupSubMenuModel menu2(nullptr, nullptr, model, 1);
  EXPECT_EQ(3u, menu2.GetItemCount());

  ExistingTabGroupSubMenuModel menu3(nullptr, nullptr, model, 2);
  EXPECT_EQ(4u, menu3.GetItemCount());
}

// Verify tabs can be added tab groups in the same window.
IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelTest,
                       AddTabsToGroupSameWindow) {
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);

  TabStripModel* model = browser()->tab_strip_model();
  model->AddToNewGroup({0});
  model->AddToNewGroup({1});
  ExistingTabGroupSubMenuModel menu(nullptr, nullptr, model, 2);
  EXPECT_EQ(4u, menu.GetItemCount());

  // Move the tab at index 2 into the group with the tab at index 0.
  menu.ExecuteExistingCommandForTesting(0);

  ASSERT_EQ(size_t(2), model->group_model()->ListTabGroups().size());
  EXPECT_EQ(2, model->group_model()
                   ->GetTabGroup(model->group_model()->ListTabGroups()[0])
                   ->tab_count());

  ExistingTabGroupSubMenuModel menu2(nullptr, nullptr, model, 2);
  EXPECT_EQ(3u, menu2.GetItemCount());
}

// Verify non-selected tabs can be added tab groups in the same window.
IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelTest,
                       AddNonSelectedTabsToTabGroup) {
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);

  TabStripModel* model = browser()->tab_strip_model();
  model->AddToNewGroup({0});
  model->AddToNewGroup({1});

  // Select the tab at index 2.
  model->SelectTabAt(2);

  // Create the menu on the tab at index 3.
  ExistingTabGroupSubMenuModel menu(nullptr, nullptr, model, 3);
  EXPECT_EQ(4u, menu.GetItemCount());

  // Move the tab at index 2 into the group with the tab at index 0.
  menu.ExecuteExistingCommandForTesting(0);

  ASSERT_EQ(size_t(2), model->group_model()->ListTabGroups().size());
  EXPECT_EQ(2, model->group_model()
                   ->GetTabGroup(model->group_model()->ListTabGroups()[0])
                   ->tab_count());

  ExistingTabGroupSubMenuModel menu2(nullptr, nullptr, model, 2);
  ExistingTabGroupSubMenuModel menu3(nullptr, nullptr, model, 3);

  EXPECT_EQ(3u, menu2.GetItemCount());
  EXPECT_EQ(4u, menu3.GetItemCount());
}

// Verify tabs can be added to tab groups in other browser windows.
IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelTest,
                       AddAllSelectedTabsToAnotherWindow) {
  Browser* new_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, browser()->profile(), true));

  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);

  chrome::AddTabAt(new_browser, GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(new_browser, GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(new_browser, GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(new_browser, GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);

  TabStripModel* model_1 = new_browser->tab_strip_model();
  TabStripModel* model_2 = browser()->tab_strip_model();

  EXPECT_EQ(model_1->count(), 4);
  EXPECT_EQ(model_2->count(), 4);

  std::unique_ptr<TabMenuModelDelegate> delegate_1 =
      CreateTabMenuModelDelegate(new_browser);

  // First tabs of each model consists of a tab group.
  model_1->AddToNewGroup({0});
  model_2->AddToNewGroup({0});

  ExistingTabGroupSubMenuModel menu_1(nullptr, delegate_1.get(), model_1, 1);

  // In order to move the 3 un-grouped tabs in `model_1` we must select those
  // tabs in addition to unselecting the grouped tab.
  for (int i = 1; i < model_1->count(); ++i) {
    model_1->SelectTabAt(i);
  }
  model_1->DeselectTabAt(0);

  const ui::ListSelectionModel::SelectedIndices selection_indices =
      model_1->selection_model().selected_indices();
  std::vector<int> selected_indices =
      std::vector<int>(selection_indices.begin(), selection_indices.end());
  EXPECT_EQ(selected_indices.size(), size_t(3));
  EXPECT_EQ(4u, menu_1.GetItemCount());

  // Move the 3 selected indices in model_1 to model_2.
  menu_1.ExecuteExistingCommandForTesting(1);
  EXPECT_EQ(model_2->count(), 7);

  // Verify the tab group in model_2 now has 4 tabs in it.
  TabGroup* group = model_2->group_model()->GetTabGroup(
      model_2->GetTabGroupForTab(0).value());
  EXPECT_EQ(group->tab_count(), 4);

  int num_selected = 0;

  for (int i = 0; i < model_2->count(); ++i) {
    if (model_2->IsTabSelected(i)) {
      ++num_selected;
    }
  }

  // Expect the number of tabs we moved from model_1 into model_2 is still 3.
  EXPECT_EQ(num_selected, 3);

  CloseBrowserSynchronously(new_browser);
}

IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelTest,
                       ShouldShowExistingTabGroups) {
  Browser* new_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, browser()->profile(), true));

  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(new_browser, GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);

  TabStripModel* model_1 = browser()->tab_strip_model();
  TabStripModel* model_2 = new_browser->tab_strip_model();

  EXPECT_EQ(model_1->count(), 2);
  EXPECT_EQ(model_2->count(), 1);

  ASSERT_EQ(model_1->group_model()->ListTabGroups().size(), 0U);
  ASSERT_EQ(model_2->group_model()->ListTabGroups().size(), 0U);

  // create tab group in first browser window
  model_1->AddToNewGroup({0});

  ASSERT_EQ(model_1->group_model()->ListTabGroups().size(), 1U);
  ASSERT_EQ(model_2->group_model()->ListTabGroups().size(), 0U);

  std::unique_ptr<TabMenuModelDelegate> delegate_1 =
      CreateTabMenuModelDelegate(new_browser);

  ExistingTabGroupSubMenuModel menu_1(nullptr, delegate_1.get(), model_1, 1);
  ExistingTabGroupSubMenuModel menu_2(nullptr, delegate_1.get(), model_2, 0);

  EXPECT_EQ(3u, menu_1.GetItemCount());
  EXPECT_EQ(3u, menu_2.GetItemCount());

  EXPECT_FALSE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
      model_1, 0, delegate_1.get()));
  EXPECT_TRUE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
      model_1, 1, delegate_1.get()));
  EXPECT_TRUE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
      model_2, 0, delegate_1.get()));

  CloseBrowserSynchronously(new_browser);
}

// Verify tab groups are display in the order they were created
IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelTest,
                       ShowTabGroupsInTheOrderTheyWereAdded) {
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);

  TabStripModel* model = browser()->tab_strip_model();
  std::vector<tab_groups::TabGroupId> group_ids;

  group_ids.emplace_back(model->AddToNewGroup({0}));
  group_ids.emplace_back(model->AddToNewGroup({1}));
  group_ids.emplace_back(model->AddToNewGroup({2}));
  group_ids.emplace_back(model->AddToNewGroup({3}));

  ASSERT_EQ(model->group_model()->ListTabGroups().size(), size_t(4u));

  ASSERT_EQ(model->group_model()->ListTabGroups(), group_ids);
}
// Verify that pinned tabs added to a group in another window maintain
// their selection state and are inserted in the correct position.
IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelTest,
                       AddPinnedTabsToTabGroup) {
  // Window 1: 3 tabs, the first two in a group.
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);

  TabStripModel* model_1 = browser()->tab_strip_model();
  model_1->AddToNewGroup({0, 1});
  EXPECT_EQ(model_1->count(), 3);

  // Window 2: 5 tabs, 3 pinned; tabs 0, 2, and 4 are selected.
  Browser* browser_2 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, browser()->profile(), true));

  chrome::AddTabAt(browser_2, GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser_2, GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser_2, GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser_2, GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);
  chrome::AddTabAt(browser_2, GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);

  TabStripModel* model_2 = browser_2->tab_strip_model();
  // Pin all tabs
  model_2->SetTabPinned(0, true);
  model_2->SetTabPinned(1, true);
  model_2->SetTabPinned(2, true);
  model_2->SetTabPinned(3, true);
  model_2->SetTabPinned(4, true);
  // Select the tabs 0, 2, 4
  model_2->SelectTabAt(0);
  model_2->SelectTabAt(2);
  model_2->SelectTabAt(4);
  EXPECT_EQ(model_2->count(), 5);

  std::unique_ptr<TabMenuModelDelegate> delegate_1 =
      CreateTabMenuModelDelegate(browser_2);
  ExistingTabGroupSubMenuModel menu_1(nullptr, delegate_1.get(), model_2, 0);

  // Move the selected tabs from Window 2 to the group in Window 1.
  menu_1.ExecuteExistingCommandForTesting(0);

  // Window 1 should now have 6 tabs.
  EXPECT_EQ(6, model_1->count());
  // Window 2 should now have 2 tabs.
  EXPECT_EQ(2, model_2->count());
  // Verify the selection state of the moved tabs.
  EXPECT_TRUE(model_1->IsTabSelected(2));
  EXPECT_TRUE(model_1->IsTabSelected(3));
  EXPECT_TRUE(model_1->IsTabSelected(4));

  CloseBrowserSynchronously(browser_2);
}

class ExistingTabGroupSubMenuModelClosedSavedGroupsTest
    : public InProcessBrowserTest {
 public:
  ExistingTabGroupSubMenuModelClosedSavedGroupsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kTabGroupMenuMoreEntryPoints);
  }

  void WaitForTabSyncServiceInitialization() {
    // Make the observer
    tab_groups::TabGroupSyncServiceInitializedObserver tgss_observer{
        tab_group_sync_service()};
    tgss_observer.Wait();
  }

  tab_groups::TabGroupSyncService* tab_group_sync_service() {
    return tab_groups::TabGroupSyncServiceFactory::GetForProfile(
        browser()->profile());
  }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  TabStrip* tabstrip() {
    return views::AsViewClass<TabStripRegionView>(
               browser()->GetBrowserView().tab_strip_view())
        ->tab_strip();
  }

  tab_groups::DeletionDialogController* deletion_dialog_controller() {
    return browser()
        ->browser_window_features()
        ->tab_group_deletion_dialog_controller();
  }

  TabStripController* controller() { return tabstrip()->controller(); }

  ui::MouseEvent dummy_event = ui::MouseEvent(ui::EventType::kMousePressed,
                                              gfx::PointF(),
                                              gfx::PointF(),
                                              base::TimeTicks::Now(),
                                              0,
                                              0);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelClosedSavedGroupsTest,
                       ShowSubmenuWithOnlyOpenGroups) {
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), /*index=*/-1,
                   /*foreground=*/true);

  // Put the 0th tab in a tab group.
  TabStripModel* model = browser()->tab_strip_model();
  ASSERT_EQ(model->group_model()->ListTabGroups().size(), 0U);
  model->AddToNewGroup({0});
  ASSERT_EQ(model->group_model()->ListTabGroups().size(), 1U);
  ASSERT_TRUE(model->GetTabGroupForTab(0).has_value());
  ASSERT_FALSE(model->GetTabGroupForTab(1).has_value());
  ASSERT_EQ(model->count(), 2);

  std::unique_ptr<TabMenuModelDelegate> tab_menu_model_delegate =
      CreateTabMenuModelDelegate(browser());

  EXPECT_EQ(model->group_model()->ListTabGroups().size(), 1u);

  // Submenu shows only on tab without a group.
  EXPECT_FALSE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
      model, 0, tab_menu_model_delegate.get()));
  EXPECT_TRUE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
      model, 1, tab_menu_model_delegate.get()));

  ExistingTabGroupSubMenuModel tab_group_submenu_1(
      nullptr, tab_menu_model_delegate.get(), model, 1);

  EXPECT_EQ(tab_group_submenu_1.GetDisplayedGroupCount(), 1u);
}

IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelClosedSavedGroupsTest,
                       ShowSubmenuWithOnlyClosedGroups) {
  WaitForTabSyncServiceInitialization();
  std::unique_ptr<TabMenuModelDelegate> tab_menu_model_delegate =
      CreateTabMenuModelDelegate(browser());
  tab_groups::TabGroupSyncService* tgss =
      tab_menu_model_delegate.get()->GetTabGroupSyncService();
  ASSERT_TRUE(tgss);

  tab_groups::SavedTabGroup saved_1 =
      tab_groups::test::CreateTestSavedTabGroup(std::nullopt);
  tab_groups::SavedTabGroup saved_2 =
      tab_groups::test::CreateTestSavedTabGroup(std::nullopt);

  tgss->AddGroup(saved_1);
  tgss->AddGroup(saved_2);

  TabStripModel* model = browser()->tab_strip_model();
  ExistingTabGroupSubMenuModel tab_group_submenu(
      nullptr, tab_menu_model_delegate.get(), model, 0);

  // Check that there are 2 saved tab groups.
  EXPECT_EQ(tgss->ReadAllGroups().size(), 2u);

  // Check that there are no open tab groups.
  EXPECT_EQ(model->group_model()->ListTabGroups().size(), 0u);

  // Check that there are exactly 2 groups in the submenu.
  EXPECT_EQ(tab_group_submenu.GetDisplayedGroupCount(), 2u);

  EXPECT_TRUE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
      model, 0, tab_menu_model_delegate.get()));
}

IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelClosedSavedGroupsTest,
                       ShowClosedAndOpenGroups) {
  WaitForTabSyncServiceInitialization();
  std::unique_ptr<TabMenuModelDelegate> tab_menu_model_delegate =
      CreateTabMenuModelDelegate(browser());
  tab_groups::TabGroupSyncService* tgss =
      tab_menu_model_delegate.get()->GetTabGroupSyncService();
  ASSERT_TRUE(tgss);

  // Make two closed saved tab groups.
  tab_groups::SavedTabGroup saved_1 =
      tab_groups::test::CreateTestSavedTabGroup(std::nullopt);
  tab_groups::SavedTabGroup saved_2 =
      tab_groups::test::CreateTestSavedTabGroup(std::nullopt);

  tgss->AddGroup(saved_1);
  tgss->AddGroup(saved_2);

  // Make 4 tabs and put the first three in groups.
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), -1, false);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), -1, false);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), -1, false);

  TabStripModel* model = browser()->tab_strip_model();
  ASSERT_EQ(model->count(), 4);

  model->AddToNewGroup({0});
  model->AddToNewGroup({1});
  model->AddToNewGroup({2});

  // Check that there are exactly 4 tab groups shown in the menu, because
  // we have 2 open tab groups and 2 closed saved tab groups.
  // Note that we only count 2 open groups because the 0-th tab is
  // already in a group, and that group should not show in the tab group
  // submenu.
  ExistingTabGroupSubMenuModel tab_group_submenu_0(
      nullptr, tab_menu_model_delegate.get(), model, 0);
  EXPECT_TRUE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
      model, 0, tab_menu_model_delegate.get()));
  EXPECT_EQ(tab_group_submenu_0.GetDisplayedGroupCount(), 4u);

  // The tab at index 3 has no group, so the submenu should have exactly
  // 1 more menu item.
  ExistingTabGroupSubMenuModel tab_group_submenu_3(
      nullptr, tab_menu_model_delegate.get(), model, 3);
  EXPECT_TRUE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
      model, 3, tab_menu_model_delegate.get()));
  EXPECT_EQ(tab_group_submenu_3.GetDisplayedGroupCount(), 5u);

  // Check that the submenu is showing on every tab, because we have saved tab
  // groups.
  EXPECT_TRUE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
      model, 0, tab_menu_model_delegate.get()));
  EXPECT_TRUE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
      model, 1, tab_menu_model_delegate.get()));
  EXPECT_TRUE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
      model, 2, tab_menu_model_delegate.get()));
  EXPECT_TRUE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
      model, 3, tab_menu_model_delegate.get()));
}

IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelClosedSavedGroupsTest,
                       AddEntireOpenSavedGroupToClosedSavedGroup) {
  WaitForTabSyncServiceInitialization();

  // Make a closed saved tab group.
  tab_groups::SavedTabGroup closed_saved_group =
      tab_groups::test::CreateTestSavedTabGroup(std::nullopt);
  tab_group_sync_service()->AddGroup(closed_saved_group);

  // Prepare tabs to add to the closed saved group. We will prepare to add
  // the tabs of an entire group and a partial group. Both of these
  // should be saved and open in the tab strip.
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), -1, false);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), -1, false);
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), -1, false);

  ASSERT_TRUE(tab_strip_model()->count() == 4);

  tab_groups::TabGroupId blue = tab_strip_model()->AddToNewGroup({0});
  tab_groups::TabGroupId green = tab_strip_model()->AddToNewGroup({1, 2});

  // Check that these are also saved groups
  ASSERT_TRUE(tab_group_sync_service()->GetGroup(blue));
  ASSERT_TRUE(tab_group_sync_service()->GetGroup(green));

  // Select all of |blue|, but only one tab of |green|.
  controller()->SelectTab(0, dummy_event);
  controller()->ExtendSelectionTo(1);

  // Now add all these to the closed saved tab group with the submenu.
  ASSERT_TRUE(controller()->IsActiveTab(1));
  ASSERT_TRUE(controller()->IsTabSelected(0));
  ASSERT_TRUE(controller()->IsTabSelected(1));
  ASSERT_FALSE(controller()->IsTabSelected(2));

  std::unique_ptr<TabMenuModelDelegate> delegate =
      CreateTabMenuModelDelegate(browser());
  ExistingTabGroupSubMenuModel tab_group_submenu(nullptr, delegate.get(),
                                                 tab_strip_model(), 1);

  // Only two groups are shown, the closed saved tab group and the group
  // only partially covered by the selected tabs (namely |green|)
  ASSERT_EQ(tab_group_submenu.GetDisplayedGroupCount(), 2u);
  tab_group_submenu.ExecuteExistingCommandForTesting(1);

  // Hit OK on the dialog if it is showing
  if (deletion_dialog_controller()->IsShowingDialog()) {
    deletion_dialog_controller()->SimulateOkButtonForTesting();
  }

  // Check that the group we selected entirely to add to the closed saved group
  // is deleted, both locally and in the sync service. Check the same is not
  // true for the group which we only partially added.

  EXPECT_FALSE(tab_strip_model()->group_model()->ContainsTabGroup(blue));
  EXPECT_TRUE(tab_strip_model()->group_model()->ContainsTabGroup(green));

  EXPECT_FALSE(tab_group_sync_service()->GetGroup(blue));
  EXPECT_TRUE(tab_group_sync_service()->GetGroup(green));

  // Finally make sure our closed saved group was updated.
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_sync_service()->GetGroup(closed_saved_group.saved_guid());

  ASSERT_TRUE(group.has_value());
  ASSERT_EQ(group->saved_tabs().size(),
            closed_saved_group.saved_tabs().size() + 2u);
}

IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelClosedSavedGroupsTest,
                       AddEntireOpenSavedGroupTabGroupDeletionDialogCancel) {
  WaitForTabSyncServiceInitialization();

  // Make a closed saved tab group.
  tab_groups::SavedTabGroup closed_saved_group =
      tab_groups::test::CreateTestSavedTabGroup(std::nullopt);
  tab_group_sync_service()->AddGroup(closed_saved_group);

  // Make a local tab group.
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), -1, true);

  ASSERT_EQ(tab_strip_model()->count(), 2);
  tab_groups::TabGroupId blue = tab_strip_model()->AddToNewGroup({1});

  tab_strip_model()->SelectTabAt(1);

  // Add the local tab group to the closed saved tab group using the submenu.
  // Make sure the dialog shows.
  std::unique_ptr<TabMenuModelDelegate> delegate =
      CreateTabMenuModelDelegate(browser());
  ExistingTabGroupSubMenuModel tab_group_submenu(nullptr, delegate.get(),
                                                 tab_strip_model(), 1);

  ASSERT_EQ(tab_group_submenu.GetDisplayedGroupCount(), 1u);
  deletion_dialog_controller()->SetPrefsPreventShowingDialogForTesting(
      /*should_prevent_dialog*/ false);
  tab_group_submenu.ExecuteExistingCommandForTesting(0);

  // Check that the dialog is showing
  ASSERT_TRUE(deletion_dialog_controller()->IsShowingDialog());
  deletion_dialog_controller()->SimulateCancelButtonForTesting();
  ASSERT_FALSE(deletion_dialog_controller()->IsShowingDialog());

  // Check that nothing happened, the groups still exist and the closed saved
  // tab group did not get any new tabs.
  EXPECT_TRUE(tab_strip_model()->group_model()->ContainsTabGroup(blue));
  EXPECT_EQ(tab_strip_model()->count(), 2);

  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_sync_service()->GetGroup(closed_saved_group.saved_guid());

  ASSERT_TRUE(group.has_value());
  ASSERT_EQ(group->saved_tabs().size(), closed_saved_group.saved_tabs().size());
}

IN_PROC_BROWSER_TEST_F(ExistingTabGroupSubMenuModelClosedSavedGroupsTest,
                       AddAllTabsToClosedSavedGroup) {
  WaitForTabSyncServiceInitialization();

  // Make a closed saved tab group.
  tab_groups::SavedTabGroup closed_saved_group =
      tab_groups::test::CreateTestSavedTabGroup(std::nullopt);
  tab_group_sync_service()->AddGroup(closed_saved_group);

  chrome::AddTabAt(browser(), GURL("chrome://newtab"), -1, true);

  // Select all the tabs, and add it the closed saved group.
  controller()->SelectTab(0, dummy_event);
  controller()->ExtendSelectionTo(1);
  ASSERT_TRUE(controller()->IsActiveTab(1));

  std::unique_ptr<TabMenuModelDelegate> delegate =
      CreateTabMenuModelDelegate(browser());
  ExistingTabGroupSubMenuModel tab_group_submenu(nullptr, delegate.get(),
                                                 tab_strip_model(), 1);

  ASSERT_EQ(tab_group_submenu.GetDisplayedGroupCount(), 1u);
  tab_group_submenu.ExecuteExistingCommandForTesting(0);

  // After adding all tabs to the closed saved tab group, we should have a
  // new tab that was added so the window does not close.
  EXPECT_EQ(tab_strip_model()->count(), 1);

  // Check the closed saved group was updated as well.
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_sync_service()->GetGroup(closed_saved_group.saved_guid());

  ASSERT_TRUE(group.has_value());
  ASSERT_EQ(group->saved_tabs().size(),
            closed_saved_group.saved_tabs().size() + 2u);
}
