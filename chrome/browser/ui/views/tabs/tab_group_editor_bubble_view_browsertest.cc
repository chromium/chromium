// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/test/button_test_api.h"

class TabGroupEditorBubbleViewDialogBrowserTest : public DialogBrowserTest {
 protected:
  void ShowUi(const std::string& name) override {
    absl::optional<tab_groups::TabGroupId> group =
        browser()->tab_strip_model()->AddToNewGroup({0});
    browser()->tab_strip_model()->OpenTabGroupEditor(group.value());

    BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
    TabGroupHeader* header =
        browser_view->tabstrip()->group_header(group.value());
    ASSERT_NE(nullptr, header);
    ASSERT_TRUE(header->editor_bubble_tracker_.is_open());
  }

  static views::Widget* GetEditorBubbleWidget(const TabGroupHeader* header) {
    return header->editor_bubble_tracker_.is_open()
               ? header->editor_bubble_tracker_.widget()
               : nullptr;
  }
};

#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_default DISABLED_InvokeUi_default
#else
#define MAYBE_InvokeUi_default InvokeUi_default
#endif
IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       MAYBE_InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       NewTabInGroup) {
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

  ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(new_tab_button).NotifyClick(released_event);

  EXPECT_EQ(2u, group_model->GetTabGroup(group_list[0])->ListTabs().length());
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest, Ungroup) {
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

  ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(ungroup_button).NotifyClick(released_event);

  EXPECT_EQ(0u, group_model->ListTabGroups().size());
  EXPECT_FALSE(group_model->ContainsTabGroup(group_list[0]));
  EXPECT_EQ(1, tsm->count());
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       CloseGroupClosesBrowser) {
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

  ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(close_group_button).NotifyClick(released_event);

  EXPECT_EQ(0u, group_model->ListTabGroups().size());
  EXPECT_FALSE(group_model->ContainsTabGroup(group_list[0]));
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_TRUE(browser()->IsAttemptingToCloseBrowser());
}

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleViewDialogBrowserTest,
                       MoveGroupToNewWindow) {
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

  views::Button* const move_group_button =
      views::Button::AsButton(editor_bubble->GetContentsView()->GetViewByID(
          TabGroupEditorBubbleView::
              TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW));
  EXPECT_NE(nullptr, move_group_button);

  ui::MouseEvent released_event(ui::ET_MOUSE_RELEASED, gfx::PointF(),
                                gfx::PointF(), base::TimeTicks(), 0, 0);
  views::test::ButtonTestApi(move_group_button).NotifyClick(released_event);

  EXPECT_EQ(0u, group_model->ListTabGroups().size());
  EXPECT_FALSE(group_model->ContainsTabGroup(group_list[0]));
  EXPECT_EQ(0, browser()->tab_strip_model()->count());

  BrowserList* browser_list = BrowserList::GetInstance();
  Browser* active_browser = browser_list->GetLastActive();
  ASSERT_NE(active_browser, browser());
  EXPECT_EQ(1, active_browser->tab_strip_model()->count());
  EXPECT_EQ(
      1u,
      active_browser->tab_strip_model()->group_model()->ListTabGroups().size());
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
  absl::optional<tab_groups::TabGroupId> group = tsm->AddToNewGroup({0, 1});

  ASSERT_FALSE(browser_view->tabstrip()->tab_at(0)->HasFreezingVoteToken());
  ASSERT_FALSE(browser_view->tabstrip()->tab_at(1)->HasFreezingVoteToken());
  ASSERT_FALSE(browser_view->tabstrip()->tab_at(2)->HasFreezingVoteToken());
  browser_view->tabstrip()->ToggleTabGroupCollapsedState(group.value());
  EXPECT_TRUE(browser_view->tabstrip()->tab_at(0)->HasFreezingVoteToken());
  EXPECT_TRUE(browser_view->tabstrip()->tab_at(1)->HasFreezingVoteToken());
  EXPECT_FALSE(browser_view->tabstrip()->tab_at(2)->HasFreezingVoteToken());
}
