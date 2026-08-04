// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/tab/tab_close_button.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/label_button.h"

class TabCollectionNodeInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest>,
      public testing::WithParamInterface<TabStripOrientation> {
 public:
  TabCollectionNodeInteractiveUiTest() = default;
  ~TabCollectionNodeInteractiveUiTest() override = default;

  TabStripOrientation orientation() const { return GetParam(); }
  bool is_horizontal() const {
    return orientation() == TabStripOrientation::kHorizontal;
  }

  void SetUpOnMainThread() override {
    VerticalTabsInteractiveTestMixin<
        InteractiveBrowserTest>::SetUpOnMainThread();
    if (is_horizontal()) {
      ExitVerticalTabsMode();
    }
  }

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    auto enabled = VerticalTabsInteractiveTestMixin<
        InteractiveBrowserTest>::GetEnabledFeatures();
    enabled.push_back({tabs::kTabStripUnification, {}});
    return enabled;
  }

 protected:
  RootTabCollectionNode* GetRootNode() {
    auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
    return base_region_view ? base_region_view->root_node_for_testing()
                            : nullptr;
  }

  views::FocusManager* GetFocusManager() {
    return BrowserView::GetBrowserViewForBrowser(browser())->GetFocusManager();
  }

  gfx::NativeWindow GetWindowHint(const views::View* view) {
    return view->GetWidget() ? view->GetWidget()->GetNativeWindow()
                             : gfx::NativeWindow();
  }
};

#if BUILDFLAG(IS_WIN)
#define MAYBE_ValidateViewFocusOrder DISABLED_ValidateViewFocusOrder
#else
#define MAYBE_ValidateViewFocusOrder ValidateViewFocusOrder
#endif
IN_PROC_BROWSER_TEST_P(TabCollectionNodeInteractiveUiTest,
                       MAYBE_ValidateViewFocusOrder) {
  // Initial Order: [A, B, C, D, E, F].
  for (size_t i = 0; i < 5; i++) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url::kAboutBlankURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  // Final Order: [D, E] [[A, B, C], F].
  auto group_id = browser()->tab_strip_model()->AddToNewGroup({1, 2});
  browser()->tab_strip_model()->SetTabPinned(3, true);
  browser()->tab_strip_model()->SetTabPinned(4, true);
  browser()->tab_strip_model()->AddToExistingGroup({2}, group_id);
  browser()->tab_strip_model()->ActivateTabAt(0);

  const auto& pinned_node = GetRootNode()->children()[0];
  EXPECT_EQ(pinned_node->type(), TabCollectionNode::Type::PINNED);
  const auto& unpinned_node = GetRootNode()->children()[1];
  EXPECT_EQ(unpinned_node->type(), TabCollectionNode::Type::UNPINNED);
  const auto& group_node = unpinned_node->children()[0];
  EXPECT_EQ(group_node->type(), TabCollectionNode::Type::GROUP);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return pinned_node->children().size() == 2u; }));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return unpinned_node->children().size() == 2u; }));

  auto* group_view = views::AsViewClass<TabGroupView>(group_node->view());
  CHECK(group_view);
  views::View* editor_button =
      group_view->group_header()->editor_bubble_button();
  if (editor_button) {
    editor_button->SetVisible(true);
  }

  // Focus Order: D, E, A, B, C, F.
  std::vector<views::View*> views_focus_order = {
      pinned_node->children()[0]->view(), pinned_node->children()[1]->view(),
      group_view->group_header()};
  if (editor_button) {
    views_focus_order.push_back(editor_button);
  }
  views_focus_order.push_back(group_node->children()[0]->view());
  views_focus_order.push_back(group_node->children()[1]->view());
  views_focus_order.push_back(group_node->children()[2]->view());
  views_focus_order.push_back(unpinned_node->children()[1]->view());

  // Assert focus order.
  GetFocusManager()->SetKeyboardAccessible(true);
  for (size_t i = 0; i < views_focus_order.size() - 1; ++i) {
    views::View* next = GetFocusManager()->GetNextFocusableView(
        views_focus_order[i], nullptr, false, true);
    EXPECT_EQ(next, views_focus_order[i + 1]);
  }
}

IN_PROC_BROWSER_TEST_P(TabCollectionNodeInteractiveUiTest,
                       KeepsFocusWhenMovedOutOfGroup) {
  constexpr char kTabNodeViewName[] = "TabNodeView";

  RunTestSequence(
      // Setup the group.
      Do([this]() {
        GetFocusManager()->SetKeyboardAccessible(true);
        browser()->tab_strip_model()->AddToNewGroup({0});
        RunScheduledLayouts();
      }),

      NameView(kTabNodeViewName,
               base::BindLambdaForTesting([this]() -> views::View* {
                 TabCollectionNode* group_node =
                     unpinned_collection_node()->GetChildNodeOfType(
                         TabCollectionNode::Type::GROUP);
                 return group_node->children()[0]->view();
               })),

      // Request Focus on Tab.
      WithView(kTabNodeViewName,
               [this](views::View* view) {
                 EXPECT_TRUE(
                     ui_test_utils::BringBrowserWindowToFront(browser()));
                 view->RequestFocus();
               }),

      // Wait for Focus on Tab.
      CheckViewProperty(kTabNodeViewName, &views::View::HasFocus, true),

      // Remove the tab from the group.
      Do([this]() {
        browser()->tab_strip_model()->RemoveFromGroup({0});
        RunScheduledLayouts();
      }),

      // Verify focus is maintained after reparenting.
      CheckResult(base::BindLambdaForTesting([this]() {
                    // After being removed from the group, the tab is back in
                    // the unpinned collection.
                    return unpinned_collection_node()
                        ->children()[0]
                        ->view()
                        ->HasFocus();
                  }),
                  true));
}

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/532713867): Re-enable this test on Windows.
#define MAYBE_ClosingTabsUpdatesHoverState DISABLED_ClosingTabsUpdatesHoverState
#else
#define MAYBE_ClosingTabsUpdatesHoverState ClosingTabsUpdatesHoverState
#endif
IN_PROC_BROWSER_TEST_P(TabCollectionNodeInteractiveUiTest,
                       MAYBE_ClosingTabsUpdatesHoverState) {
  for (size_t i = 0; i < 2; ++i) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url::kAboutBlankURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  TabStripModel* model = browser()->tab_strip_model();
  ASSERT_EQ(model->count(), 3);

  const auto& unpinned_node = GetRootNode()->children()[1];

  auto* first_tab =
      views::AsViewClass<TabView>(unpinned_node->children()[0]->view());
  auto* second_tab =
      views::AsViewClass<TabView>(unpinned_node->children()[1]->view());

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !first_tab->bounds().IsEmpty(); }));

  // Move the mouse position over the first tab.
  EXPECT_TRUE(ui_test_utils::SendMouseMoveSync(
      first_tab->GetBoundsInScreen().CenterPoint(), GetWindowHint(first_tab)));
  EXPECT_TRUE(ui_test_utils::SendMouseMoveSync(
      first_tab->GetBoundsInScreen().CenterPoint() + gfx::Vector2d(1, 0),
      GetWindowHint(first_tab)));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return first_tab->close_button_for_testing()->GetVisible(); }));

  // Close the tab that is currently hovered.
  model->CloseWebContentsAt(0, TabCloseTypes::CLOSE_NONE);

  // Check if the hover state is on the second tab.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return second_tab->close_button_for_testing()->GetVisible(); }));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TabCollectionNodeInteractiveUiTest,
    testing::Values(TabStripOrientation::kVertical,
                    TabStripOrientation::kHorizontal),
    [](const testing::TestParamInfo<TabStripOrientation>& info) {
      switch (info.param) {
        case TabStripOrientation::kVertical:
          return "Vertical";
        case TabStripOrientation::kHorizontal:
          return "Horizontal";
      }
    });
