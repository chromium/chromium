// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_container.h"
#include <memory>

#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/fake_tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/test/views/chrome_views_test_base.h"

class TabContainerTest : public ChromeViewsTestBase {
 public:
  TabContainerTest() = default;
  TabContainerTest(const TabContainerTest&) = delete;
  TabContainerTest& operator=(const TabContainerTest&) = delete;
  ~TabContainerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    tab_strip_controller_ = std::make_unique<FakeBaseTabStripController>();
    tab_container_ = std::make_unique<TabContainer>(
        tab_strip_controller_.get(), nullptr /*hover_card_controller*/,
        nullptr /*drag_context*/, nullptr /*scroll_contents_view*/);
    tab_container_->SetAvailableWidthCallback(
        base::BindRepeating([]() { return 500; }));
    tab_controller_ = std::make_unique<FakeTabController>();
  }

  void TearDown() override {
    ChromeViewsTestBase::TearDown();
    tab_strip_controller_.reset();
    tab_container_.reset();
    tab_controller_.reset();
  }

 protected:
  // Returns all TabSlotViews in the order that they have as ViewChildren of
  // TabContainer. This should match the actual order that they appear in
  // visually.
  views::View::Views GetTabSlotViewsInFocusOrder() {
    views::View::Views all_children = tab_container_->children();

    const int num_tab_slot_views = tab_container_->GetTabCount();

    return views::View::Views(all_children.begin(),
                              all_children.begin() + num_tab_slot_views);
  }

  // Returns all TabSlotViews in the order that they appear visually. This is
  // the expected order of the ViewChildren of TabContainer.
  views::View::Views GetTabSlotViewsInVisualOrder() {
    views::View::Views ordered_views;

    for (int i = 0; i < tab_container_->GetTabCount(); ++i) {
      Tab* tab = tab_container_->GetTabAtModelIndex(i);

      ordered_views.push_back(tab);
    }

    return ordered_views;
  }

  std::unique_ptr<FakeBaseTabStripController> tab_strip_controller_;
  std::unique_ptr<FakeTabController> tab_controller_;
  std::unique_ptr<TabContainer> tab_container_;
};

// Verifies child view order matches model order.
TEST_F(TabContainerTest, TabViewOrder) {
  tab_container_->AddTab(std::make_unique<Tab>(tab_controller_.get()), 0,
                         TabPinned::kUnpinned);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  tab_container_->AddTab(std::make_unique<Tab>(tab_controller_.get()), 1,
                         TabPinned::kUnpinned);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  tab_container_->AddTab(std::make_unique<Tab>(tab_controller_.get()), 2,
                         TabPinned::kUnpinned);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());

  tab_container_->MoveTab(tab_container_->GetTabAtModelIndex(0), 0, 1);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  tab_container_->MoveTab(tab_container_->GetTabAtModelIndex(1), 1, 2);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  tab_container_->MoveTab(tab_container_->GetTabAtModelIndex(1), 1, 0);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  tab_container_->MoveTab(tab_container_->GetTabAtModelIndex(0), 0, 2);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
}
