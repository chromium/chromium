// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/data_sharing/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_image_container.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

class TabGroupEditorBubbleViewDialogBrowserTest : public DialogBrowserTest {
 public:
  TabGroupEditorBubbleViewDialogBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kBookmarkTabGroupConversion},
        {data_sharing::features::kDataSharingFeature,
         data_sharing::features::kDataSharingJoinOnly});
  }

 protected:
  void ShowUi(const std::string& name) override {
    group_ = browser()->tab_strip_model()->AddToNewGroup({0});
    browser()->tab_strip_model()->OpenTabGroupEditor(group_.value());
  }

  static views::Widget* WaitForAndGetEditorBubbleWidget() {
    auto bubble_view =
        views::ElementTrackerViews::GetInstance()
            ->GetAllMatchingViewsInAnyContext(
                TabGroupEditorBubbleView::kTabGroupEditorBubbleViewId);
    if (bubble_view.empty()) {
      return nullptr;
    }

    views::Widget* const widget = bubble_view[0]->GetWidget();
    views::test::WidgetVisibleWaiter(widget).Wait();
    return widget;
  }

  TabGroupModel* group_model() {
    return browser()->tab_strip_model()->group_model();
  }

  std::optional<tab_groups::TabGroupId> group_;
  base::test::ScopedFeatureList scoped_feature_list_;
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

  views::Widget* editor_bubble = WaitForAndGetEditorBubbleWidget();
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
  if (browser()->GetFeatures().tab_group_deletion_dialog_controller()) {
    browser()
        ->GetFeatures()
        .tab_group_deletion_dialog_controller()
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

  views::Widget* editor_bubble = WaitForAndGetEditorBubbleWidget();
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

// TODO(crbug.com/388544209): Flaky on linux-win-cross-rel.
#if BUILDFLAG(IS_WIN)
#define MAYBE_MoveGroupToNewWindow DISABLED_MoveGroupToNewWindow
#else
#define MAYBE_MoveGroupToNewWindow MoveGroupToNewWindow
#endif
IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       MAYBE_MoveGroupToNewWindow) {
  // Add a tab so theres more than just the group in the tabstrip
  InProcessBrowserTest::AddBlankTabAndShow(browser());

  ShowUi("SetUp");

  views::Widget* editor_bubble = WaitForAndGetEditorBubbleWidget();
  ASSERT_NE(nullptr, editor_bubble);

  views::Button* const move_group_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::
              TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW));
  EXPECT_NE(nullptr, move_group_button);

  ui::MouseEvent released_event(ui::EventType::kMouseReleased, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  views::test::ButtonTestApi(move_group_button).NotifyClick(released_event);
  ui_test_utils::WaitForBrowserSetLastActive(browser_created_observer.Wait());

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
    if (tsm->GetTabAtIndex(index)->GetGroup() != group_) {
      tsm->CloseWebContentsAt(index, TabCloseTypes::CLOSE_NONE);
    }
  }

  ShowUi("SetUp");

  views::Widget* editor_bubble = WaitForAndGetEditorBubbleWidget();
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

class TabGroupEditorBubbleViewDialogBrowserTestWithSavedGroup
    : public TabGroupEditorBubbleViewDialogBrowserTest {
 public:
  TabGroupEditorBubbleViewDialogBrowserTestWithSavedGroup() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTestWithSavedGroup,
                       UngroupSavedGroupShowsDialog) {
  base::HistogramTester histogram_tester;

  ShowUi("SetUp");

  TabStripModel* tsm = browser()->tab_strip_model();
  ASSERT_EQ(1, tsm->count());
  TabGroupModel* group_model = tsm->group_model();
  std::vector<tab_groups::TabGroupId> group_list = group_model->ListTabGroups();
  ASSERT_EQ(1u, group_list.size());
  ASSERT_EQ(1u, group_model->GetTabGroup(group_list[0])->ListTabs().length());

  views::Widget* editor_bubble = WaitForAndGetEditorBubbleWidget();
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
      browser()->GetFeatures().tab_group_deletion_dialog_controller();
  EXPECT_TRUE(deletion_dialog_controller->IsShowingDialog());

  // Pull the dialog state and call the OnDialogOk method.
  deletion_dialog_controller->SimulateOkButtonForTesting();

  // Make sure that the ungroup action occured.
  EXPECT_EQ(0u, group_model->ListTabGroups().size());
  EXPECT_FALSE(group_model->ContainsTabGroup(group_list[0]));
  EXPECT_EQ(1, tsm->count());
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTestWithSavedGroup,
                       CloseGroupedTab) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  InProcessBrowserTest::AddBlankTabAndShow(browser());
  InProcessBrowserTest::AddBlankTabAndShow(browser());

  TabStripModel* tsm = browser()->tab_strip_model();
  ASSERT_EQ(3, tsm->count());
  tsm->AddToNewGroup({0});
  tsm->ActivateTabAt(0);
  tsm->CloseSelectedTabs();
  tab_groups::DeletionDialogController* deletion_dialog_controller =
      browser_view->browser()
          ->GetFeatures()
          .tab_group_deletion_dialog_controller();

  EXPECT_TRUE(deletion_dialog_controller->IsShowingDialog());

  deletion_dialog_controller->SimulateOkButtonForTesting();

  EXPECT_FALSE(deletion_dialog_controller->IsShowingDialog());
  EXPECT_EQ(2, tsm->count());
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTestWithSavedGroup,
                       CloseGroupedTabWithPreventShowDialog) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  InProcessBrowserTest::AddBlankTabAndShow(browser());
  InProcessBrowserTest::AddBlankTabAndShow(browser());

  tab_groups::DeletionDialogController* deletion_dialog_controller =
      browser_view->browser()
          ->GetFeatures()
          .tab_group_deletion_dialog_controller();
  deletion_dialog_controller->SetPrefsPreventShowingDialogForTesting(true);

  TabStripModel* tsm = browser()->tab_strip_model();
  ASSERT_EQ(3, tsm->count());
  tsm->AddToNewGroup({0});
  tsm->ActivateTabAt(0);
  tsm->CloseSelectedTabs();

  EXPECT_FALSE(deletion_dialog_controller->IsShowingDialog());
  EXPECT_EQ(2, tsm->count());
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTestWithSavedGroup,
                       ConvertTabGroupToBookmark) {
  base::UserActionTester user_action_tester;

  ShowUi("SetUp");

  TabStripModel* tsm = browser()->tab_strip_model();
  ASSERT_EQ(1, tsm->count());
  TabGroupModel* group_model = tsm->group_model();
  std::vector<tab_groups::TabGroupId> group_list = group_model->ListTabGroups();
  ASSERT_EQ(1u, group_list.size());
  ASSERT_EQ(1u, group_model->GetTabGroup(group_list[0])->ListTabs().length());

  views::Widget* editor_bubble = WaitForAndGetEditorBubbleWidget();
  ASSERT_NE(nullptr, editor_bubble);

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       BookmarkEditorView::kViewClassName);

  // Convert the tab group to bookmark.
  views::Button* const convert_to_bookmark_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::
              TAB_GROUP_HEADER_CXMENU_CONVERT_TO_BOOKMARK));
  ASSERT_NE(nullptr, convert_to_bookmark_button);
  ui::MouseEvent released_event(ui::EventType::kMouseReleased, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(convert_to_bookmark_button)
      .NotifyClick(released_event);

  // Make sure the bookmark editor is shown and press the save button.
  views::Widget* bookmark_editor_widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(nullptr, bookmark_editor_widget);
  bookmark_editor_widget->widget_delegate()->AsDialogDelegate()->Accept();

  // Make sure that the group is removed after convert to bookmark.
  EXPECT_EQ(0u, group_model->ListTabGroups().size());

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "BookmarkTabGroupConversion_ConvertToBookmarkSelected"));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "BookmarkTabGroupConversion_ConvertToBookmarkConfirmed"));
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTestWithSavedGroup,
                       ConvertTabGroupToBookmarkDisabledByPolicy) {
  // Bookmark disabled by policy.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kEditBookmarksEnabled, false);

  ShowUi("SetUp");

  views::Widget* editor_bubble = WaitForAndGetEditorBubbleWidget();
  ASSERT_NE(nullptr, editor_bubble);

  // Make sure the convert to bookmark button is not shown.
  views::Button* const convert_to_bookmark_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::
              TAB_GROUP_HEADER_CXMENU_CONVERT_TO_BOOKMARK));
  ASSERT_EQ(nullptr, convert_to_bookmark_button);
}

class TabGroupEditorBubbleViewDialogBrowserTestWithFocusingEnabled
    : public TabGroupEditorBubbleViewDialogBrowserTest {
 public:
  TabGroupEditorBubbleViewDialogBrowserTestWithFocusingEnabled() {
    scoped_feature_list_.InitAndEnableFeature(features::kTabGroupsFocusing);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    TabGroupEditorBubbleViewDialogBrowserTestWithFocusingEnabled,
    ToggleFocusGroup) {
  ShowUi("SetUp");

  TabStripModel* const tsm = browser()->tab_strip_model();
  ASSERT_TRUE(group_.has_value());
  const tab_groups::TabGroupId group_id = group_.value();

  // 1. Initially, no group is focused.
  EXPECT_FALSE(tsm->GetFocusedGroup().has_value());

  views::Widget* editor_bubble = WaitForAndGetEditorBubbleWidget();
  ASSERT_NE(nullptr, editor_bubble);
  ASSERT_NE(nullptr, editor_bubble->GetContentsView());

  views::View* focus_button_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kTabGroupEditorBubbleFocusGroupButtonId,
          views::ElementTrackerViews::GetContextForView(
              editor_bubble->GetRootView()));
  ASSERT_NE(nullptr, focus_button_view);
  views::LabelButton* const focus_button =
      static_cast<views::LabelButton*>(focus_button_view);
  EXPECT_EQ(focus_button->GetText(),
            l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_FOCUS_GROUP));

  // 2. Click to focus the group.
  ui::MouseEvent released_event(ui::EventType::kMouseReleased, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(focus_button).NotifyClick(released_event);

  // The bubble should close. Wait for it.
  views::test::WidgetDestroyedWaiter(editor_bubble).Wait();

  EXPECT_TRUE(tsm->GetFocusedGroup().has_value());
  EXPECT_EQ(group_id, tsm->GetFocusedGroup().value());

  // 3. Open the editor again to unfocus.
  browser()->tab_strip_model()->OpenTabGroupEditor(group_id);
  views::Widget* editor_bubble2 = WaitForAndGetEditorBubbleWidget();
  ASSERT_NE(nullptr, editor_bubble2);
  ASSERT_NE(nullptr, editor_bubble2->GetContentsView());

  views::View* unfocus_button_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kTabGroupEditorBubbleUnfocusGroupButtonId,
          views::ElementTrackerViews::GetInstance()->GetContextForView(
              editor_bubble2->GetRootView()));
  ASSERT_NE(nullptr, unfocus_button_view);
  views::LabelButton* const unfocus_button =
      static_cast<views::LabelButton*>(unfocus_button_view);
  EXPECT_EQ(
      unfocus_button->GetText(),
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_UNFOCUS_GROUP));

  // 4. Click to unfocus the group.
  views::test::ButtonTestApi(unfocus_button).NotifyClick(released_event);
  views::test::WidgetDestroyedWaiter(editor_bubble2).Wait();

  EXPECT_FALSE(tsm->GetFocusedGroup().has_value());
}
