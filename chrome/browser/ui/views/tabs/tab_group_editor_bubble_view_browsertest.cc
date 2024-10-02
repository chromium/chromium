// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/button_test_api.h"

class TabGroupEditorBubbleViewDialogBrowserTest : public DialogBrowserTest {
 protected:
  void ShowUi(const std::string& name) override {
    group_ = browser()->tab_strip_model()->AddToNewGroup({0});
    browser()->tab_strip_model()->OpenTabGroupEditor(group_.value());

    BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
    TabGroupHeader* header =
        browser_view->tabstrip()->group_header(group_.value());
    ASSERT_NE(nullptr, header);
    ASSERT_TRUE(header->editor_bubble_tracker_.is_open());
  }

  static views::Widget* GetEditorBubbleWidget(const TabGroupHeader* header) {
    return header->editor_bubble_tracker_.is_open()
               ? header->editor_bubble_tracker_.widget()
               : nullptr;
  }

  TabGroupModel* group_model() {
    return browser()->tab_strip_model()->group_model();
  }

  std::optional<tab_groups::TabGroupId> group_;
};

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       NewTabInGroup) {
  base::HistogramTester histogram_tester;

  ShowUi("SetUp");

  TabGroupModel* group_model = browser()->tab_strip_model()->group_model();
  std::vector<tab_groups::TabGroupId> group_list = group_model->ListTabGroups();
  ASSERT_EQ(1u, group_list.size());
  ASSERT_EQ(1u, group_model->GetTabGroup(group_list[0])->ListTabs().length());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  TabGroupHeader* header =
      browser_view->tabstrip()->group_header(group_list[0]);
  views::Widget* editor_bubble = GetEditorBubbleWidget(header);
  ASSERT_NE(nullptr, editor_bubble);

  views::Button* const new_tab_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP));
  EXPECT_NE(nullptr, new_tab_button);

  ui::MouseEvent released_event(ui::EventType::kMouseReleased, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(new_tab_button).NotifyClick(released_event);

  EXPECT_EQ(2u, group_model->GetTabGroup(group_list[0])->ListTabs().length());

  histogram_tester.ExpectUniqueSample("TabGroups.TabGroupBubble.TabCount", 2,
                                      1);
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest, Ungroup) {
  base::HistogramTester histogram_tester;

  // Allow the Ungroup command to be immediately performed for saved groups.
  if (browser()->tab_group_deletion_dialog_controller()) {
    browser()
        ->tab_group_deletion_dialog_controller()
        ->SetPrefsPreventShowingDialogForTesting(
            /*should_prevent_dialog=*/true);
  }

  ShowUi("SetUp");

  TabStripModel* tsm = browser()->tab_strip_model();
  ASSERT_EQ(1, tsm->count());
  TabGroupModel* group_model = tsm->group_model();
  std::vector<tab_groups::TabGroupId> group_list = group_model->ListTabGroups();
  ASSERT_EQ(1u, group_list.size());
  ASSERT_EQ(1u, group_model->GetTabGroup(group_list[0])->ListTabs().length());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  TabGroupHeader* header =
      browser_view->tabstrip()->group_header(group_list[0]);
  views::Widget* editor_bubble = GetEditorBubbleWidget(header);
  ASSERT_NE(nullptr, editor_bubble);

  views::Button* const ungroup_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::TAB_GROUP_HEADER_CXMENU_UNGROUP));
  EXPECT_NE(nullptr, ungroup_button);

  ui::MouseEvent released_event(ui::EventType::kMouseReleased, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(ungroup_button).NotifyClick(released_event);

  EXPECT_EQ(0u, group_model->ListTabGroups().size());
  EXPECT_FALSE(group_model->ContainsTabGroup(group_list[0]));
  EXPECT_EQ(1, tsm->count());

  // Should not record for 0 tabs.
  histogram_tester.ExpectTotalCount("TabGroups.TabGroupBubble.TabCount", 0);
}

class TabGroupEditorBubbleViewDialogV2DisabledBrowserTest
    : public TabGroupEditorBubbleViewDialogBrowserTest {
 public:
  TabGroupEditorBubbleViewDialogV2DisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {tab_groups::kTabGroupsSaveUIUpdate, tab_groups::kTabGroupsSaveV2});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verify that when a group that holds all of the tabs in a window is closing
// does not close the browser. Instead it should create a new tab.
IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogV2DisabledBrowserTest,
                       ClosingLastGroupInBrowserSpawnsNewTab) {
  ShowUi("SetUp");

  TabGroupModel* group_model = browser()->tab_strip_model()->group_model();
  std::vector<tab_groups::TabGroupId> group_list = group_model->ListTabGroups();
  ASSERT_EQ(1u, group_list.size());
  ASSERT_EQ(1u, group_model->GetTabGroup(group_list[0])->ListTabs().length());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  TabGroupHeader* header =
      browser_view->tabstrip()->group_header(group_list[0]);
  views::Widget* editor_bubble = GetEditorBubbleWidget(header);
  ASSERT_NE(nullptr, editor_bubble);

  views::Button* const close_group_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP));
  EXPECT_NE(nullptr, close_group_button);

  ui::MouseEvent released_event(ui::EventType::kMouseReleased, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(close_group_button).NotifyClick(released_event);

  EXPECT_EQ(0u, group_model->ListTabGroups().size());
  EXPECT_FALSE(group_model->ContainsTabGroup(group_list[0]));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_FALSE(browser()->IsAttemptingToCloseBrowser());
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       MoveGroupToNewWindow) {
  // Add a tab so theres more than just the group in the tabstrip
  InProcessBrowserTest::AddBlankTabAndShow(browser());

  ShowUi("SetUp");

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  TabGroupHeader* header =
      browser_view->tabstrip()->group_header(group_.value());
  views::Widget* editor_bubble = GetEditorBubbleWidget(header);
  ASSERT_NE(nullptr, editor_bubble);

  views::Button* const move_group_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::
              TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW));
  EXPECT_NE(nullptr, move_group_button);

  ui::MouseEvent released_event(ui::EventType::kMouseReleased, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  ui_test_utils::BrowserChangeObserver new_browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  views::test::ButtonTestApi(move_group_button).NotifyClick(released_event);
  ui_test_utils::WaitForBrowserSetLastActive(new_browser_observer.Wait());

  EXPECT_EQ(0u, group_model()->ListTabGroups().size());
  EXPECT_FALSE(group_model()->ContainsTabGroup(group_.value()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  Browser* active_browser = chrome::FindLastActive();
  ASSERT_NE(active_browser, browser());
  EXPECT_EQ(1, active_browser->tab_strip_model()->count());
  EXPECT_EQ(
      1u,
      active_browser->tab_strip_model()->group_model()->ListTabGroups().size());
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       MoveGroupToNewWindowDisabledWhenOnlyGroup) {
  TabStripModel* tsm = browser()->tab_strip_model();
  for (int index = tsm->count() - 1; index >= 0; --index) {
    if (tsm->GetTabAtIndex(index)->group() != group_) {
      tsm->CloseWebContentsAt(index, TabCloseTypes::CLOSE_NONE);
    }
  }

  ShowUi("SetUp");

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  TabGroupHeader* header =
      browser_view->tabstrip()->group_header(group_.value());
  views::Widget* editor_bubble = GetEditorBubbleWidget(header);
  ASSERT_NE(nullptr, editor_bubble);

  views::Button* const move_group_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::
              TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW));
  EXPECT_NE(nullptr, move_group_button);
  EXPECT_FALSE(move_group_button->GetVisible());
}

class TabGroupEditorBubbleViewDialogBrowserTestWithFreezingEnabled
    : public TabGroupEditorBubbleViewDialogBrowserTest {
 public:
  TabGroupEditorBubbleViewDialogBrowserTestWithFreezingEnabled() {
    scoped_feature_list_.InitWithFeatures(
        {features::kTabGroupsCollapseFreezing}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    TabGroupEditorBubbleViewDialogBrowserTestWithFreezingEnabled,
    CollapsingGroupFreezesAllTabs) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  InProcessBrowserTest::AddBlankTabAndShow(browser());
  InProcessBrowserTest::AddBlankTabAndShow(browser());

  TabStripModel* tsm = browser()->tab_strip_model();
  ASSERT_EQ(3, tsm->count());
  std::optional<tab_groups::TabGroupId> group = tsm->AddToNewGroup({0, 1});

  ASSERT_FALSE(browser_view->tabstrip()->tab_at(0)->HasFreezingVote());
  ASSERT_FALSE(browser_view->tabstrip()->tab_at(1)->HasFreezingVote());
  ASSERT_FALSE(browser_view->tabstrip()->tab_at(2)->HasFreezingVote());
  browser_view->tabstrip()->ToggleTabGroupCollapsedState(group.value());
  EXPECT_TRUE(browser_view->tabstrip()->tab_at(0)->HasFreezingVote());
  EXPECT_TRUE(browser_view->tabstrip()->tab_at(1)->HasFreezingVote());
  EXPECT_FALSE(browser_view->tabstrip()->tab_at(2)->HasFreezingVote());
}

class TabGroupEditorBubbleViewDialogBrowserTestWithSavedGroupV2
    : public TabGroupEditorBubbleViewDialogBrowserTest {
 public:
  TabGroupEditorBubbleViewDialogBrowserTestWithSavedGroupV2() {
    scoped_feature_list_.InitWithFeatures(
        {tab_groups::kTabGroupsSaveUIUpdate, tab_groups::kTabGroupsSaveV2}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    TabGroupEditorBubbleViewDialogBrowserTestWithSavedGroupV2,
    UngroupSavedGroupShowsDialog) {
  base::HistogramTester histogram_tester;

  ShowUi("SetUp");

  TabStripModel* tsm = browser()->tab_strip_model();
  ASSERT_EQ(1, tsm->count());
  TabGroupModel* group_model = tsm->group_model();
  std::vector<tab_groups::TabGroupId> group_list = group_model->ListTabGroups();
  ASSERT_EQ(1u, group_list.size());
  ASSERT_EQ(1u, group_model->GetTabGroup(group_list[0])->ListTabs().length());

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  TabGroupHeader* header =
      browser_view->tabstrip()->group_header(group_list[0]);
  views::Widget* editor_bubble = GetEditorBubbleWidget(header);
  ASSERT_NE(nullptr, editor_bubble);

  {  // Ungroup the group.
    views::Button* const ungroup_button =
        views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
            TabGroupEditorBubbleView::TAB_GROUP_HEADER_CXMENU_UNGROUP));
    ASSERT_NE(nullptr, ungroup_button);
    ui::MouseEvent released_event(ui::EventType::kMouseReleased, gfx::PointF(),
                                  gfx::PointF(), base::TimeTicks(), 0, 0);
    views::test::ButtonTestApi(ungroup_button).NotifyClick(released_event);
  }

  // Make sure that the ungroup action did not occur.
  EXPECT_EQ(1u, group_model->ListTabGroups().size());
  EXPECT_TRUE(group_model->ContainsTabGroup(group_list[0]));
  EXPECT_EQ(1, tsm->count());

  // Make sure the dialog is shown, and fake clicking the button.
  tab_groups::DeletionDialogController* deletion_dialog_controller =
      browser_view->browser()->tab_group_deletion_dialog_controller();
  EXPECT_TRUE(deletion_dialog_controller->IsShowingDialog());

  // Pull the dialog state and call the OnDialogOk method.
  deletion_dialog_controller->SimulateOkButtonForTesting();

  // Make sure that the ungroup action occured.
  EXPECT_EQ(0u, group_model->ListTabGroups().size());
  EXPECT_FALSE(group_model->ContainsTabGroup(group_list[0]));
  EXPECT_EQ(1, tsm->count());
}

IN_PROC_BROWSER_TEST_F(
    TabGroupEditorBubbleViewDialogBrowserTestWithSavedGroupV2,
    CloseGroupedTab) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  InProcessBrowserTest::AddBlankTabAndShow(browser());
  InProcessBrowserTest::AddBlankTabAndShow(browser());

  TabStripModel* tsm = browser()->tab_strip_model();
  ASSERT_EQ(3, tsm->count());
  tsm->AddToNewGroup({0});
  browser_view->tabstrip()->CloseTab(browser_view->tabstrip()->tab_at(0),
                                     CloseTabSource::CLOSE_TAB_FROM_MOUSE);

  tab_groups::DeletionDialogController* deletion_dialog_controller =
      browser_view->browser()->tab_group_deletion_dialog_controller();

  EXPECT_TRUE(deletion_dialog_controller->IsShowingDialog());

  deletion_dialog_controller->SimulateOkButtonForTesting();

  EXPECT_FALSE(deletion_dialog_controller->IsShowingDialog());
  EXPECT_EQ(2, tsm->count());
}

IN_PROC_BROWSER_TEST_F(
    TabGroupEditorBubbleViewDialogBrowserTestWithSavedGroupV2,
    CloseGroupedTabWithPreventShowDialog) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  InProcessBrowserTest::AddBlankTabAndShow(browser());
  InProcessBrowserTest::AddBlankTabAndShow(browser());

  tab_groups::DeletionDialogController* deletion_dialog_controller =
      browser_view->browser()->tab_group_deletion_dialog_controller();
  deletion_dialog_controller->SetPrefsPreventShowingDialogForTesting(true);

  TabStripModel* tsm = browser()->tab_strip_model();
  ASSERT_EQ(3, tsm->count());
  tsm->AddToNewGroup({0});
  browser_view->tabstrip()->CloseTab(browser_view->tabstrip()->tab_at(0),
                                     CloseTabSource::CLOSE_TAB_FROM_MOUSE);

  EXPECT_FALSE(deletion_dialog_controller->IsShowingDialog());
  EXPECT_EQ(2, tsm->count());
}
