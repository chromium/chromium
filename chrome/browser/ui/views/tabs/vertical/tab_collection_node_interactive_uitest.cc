// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabCollectionNodeInteractiveUiTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  TabCollectionNodeInteractiveUiTest() = default;
  ~TabCollectionNodeInteractiveUiTest() override = default;

 protected:
  RootTabCollectionNode* GetRootNode() {
    return browser()
        ->GetBrowserView()
        .vertical_tab_strip_region_view_for_testing()
        ->root_node_for_testing();
  }

  views::FocusManager* GetFocusManager() {
    return browser()->GetBrowserView().GetFocusManager();
  }
};

IN_PROC_BROWSER_TEST_F(TabCollectionNodeInteractiveUiTest,
                       ValidateViewFocusOrder) {
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

  auto* group_view =
      views::AsViewClass<VerticalTabGroupView>(group_node->view());
  CHECK(group_view);
  group_view->group_header()->editor_bubble_button()->SetVisible(true);

  // Focus Order: D, E, A, B, C, F.
  const std::vector<views::View*> views_focus_order = {
      pinned_node->children()[0]->view(),
      pinned_node->children()[1]->view(),
      group_view->group_header(),
      group_view->group_header()->editor_bubble_button(),
      group_node->children()[0]->view(),
      group_node->children()[1]->view(),
      group_node->children()[2]->view(),
      unpinned_node->children()[1]->view()};

  // Assert focus order.
  GetFocusManager()->SetKeyboardAccessible(true);
  for (size_t i = 0; i < views_focus_order.size() - 1; ++i) {
    views::View* next = GetFocusManager()->GetNextFocusableView(
        views_focus_order[i], nullptr, false, true);
    EXPECT_EQ(next, views_focus_order[i + 1]);
  }
}
