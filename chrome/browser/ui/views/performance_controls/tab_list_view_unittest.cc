// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/tab_list_view.h"

#include <memory>
#include <optional>
#include <vector>

#include "chrome/browser/ui/performance_controls/tab_list_model.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/performance_controls/tab_list_row_view.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

class TabListViewUnitTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    pm_harness_.SetUp();
  }

  void TearDown() override {
    pm_harness_.TearDown();
    TestWithBrowserView::TearDown();
  }

  std::vector<resource_attribution::PageContext> GetPageContextAtIndices(
      std::vector<int> indices) {
    std::vector<resource_attribution::PageContext> contexts = {};
    TabStripModel* const tab_strip_model = browser()->tab_strip_model();
    for (int index : indices) {
      std::optional<resource_attribution::PageContext> context =
          resource_attribution::PageContext::FromWebContents(
              tab_strip_model->GetWebContentsAt(index));
      CHECK(context.has_value());
      contexts.emplace_back(context.value());
    }
    return contexts;
  }

  void TriggerMouseEvent(views::View* view, ui::EventType event_type) {
    ui::MouseEvent e(event_type, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    view->OnEvent(&e);
  }

 private:
  performance_manager::PerformanceManagerTestHarnessHelper pm_harness_;
};

TEST_F(TabListViewUnitTest, PopulateTabList) {
  AddTab(browser(), GURL("https://a.com"));
  AddTab(browser(), GURL("https://b.com"));

  auto tab_list_model =
      std::make_unique<TabListModel>(GetPageContextAtIndices({0, 1}));
  auto tab_list_view = std::make_unique<TabListView>(tab_list_model.get());

  std::vector<resource_attribution::PageContext> page_contexts =
      tab_list_model->page_contexts();
  auto children = tab_list_view->children();
  ASSERT_EQ(page_contexts.size(), children.size());

  TabListRowView* const first_row =
      views::AsViewClass<TabListRowView>(children[0]);
  EXPECT_EQ(first_row->GetTitleTextForTesting(), u"b.com");
  EXPECT_EQ(first_row->GetDomainTextForTesting(), u"b.com");

  TabListRowView* const second_row =
      views::AsViewClass<TabListRowView>(children[1]);
  EXPECT_EQ(second_row->GetTitleTextForTesting(), u"a.com");
  EXPECT_EQ(second_row->GetDomainTextForTesting(), u"a.com");
}

TEST_F(TabListViewUnitTest, CloseButtonRemovesListItem) {
  AddTab(browser(), GURL("https://a.com"));
  AddTab(browser(), GURL("https://b.com"));

  auto tab_list_model =
      std::make_unique<TabListModel>(GetPageContextAtIndices({0, 1}));
  auto tab_list_view = std::make_unique<TabListView>(tab_list_model.get());

  std::vector<resource_attribution::PageContext> page_contexts =
      tab_list_model->page_contexts();
  auto children = tab_list_view->children();
  ASSERT_EQ(children.size(), 2u);

  // Clicking on the X button should remove one of the children from the tab
  // list
  TabListRowView* const first_row =
      views::AsViewClass<TabListRowView>(children[0]);
  views::ImageButton* const close_button =
      first_row->GetCloseButtonForTesting();
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(close_button);
  test_api.NotifyClick(e);

  EXPECT_EQ(tab_list_view->children().size(), 1u);
  EXPECT_EQ(tab_list_model->page_contexts().size(), 1u);
}

TEST_F(TabListViewUnitTest, CloseButtonShowsAndHides) {
  AddTab(browser(), GURL("https://a.com"));
  AddTab(browser(), GURL("https://b.com"));

  auto tab_list_model =
      std::make_unique<TabListModel>(GetPageContextAtIndices({0, 1}));
  auto tab_list_view = std::make_unique<TabListView>(tab_list_model.get());

  std::vector<resource_attribution::PageContext> page_contexts =
      tab_list_model->page_contexts();
  auto children = tab_list_view->children();
  ASSERT_EQ(children.size(), 2u);

  TabListRowView* const first_row =
      views::AsViewClass<TabListRowView>(children[0]);
  views::ImageButton* const close_button =
      first_row->GetCloseButtonForTesting();

  EXPECT_FALSE(close_button->GetVisible());

  TriggerMouseEvent(first_row, ui::EventType::ET_MOUSE_ENTERED);
  EXPECT_TRUE(close_button->GetVisible());

  TriggerMouseEvent(first_row, ui::EventType::ET_MOUSE_EXITED);
  EXPECT_FALSE(close_button->GetVisible());

  TabListRowView* const second_row =
      views::AsViewClass<TabListRowView>(children[1]);
  views::ImageButton* const second_close_button =
      second_row->GetCloseButtonForTesting();

  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(second_close_button);
  test_api.NotifyClick(e);

  // The close button should now stay hidden regardless of the mouse movement
  // since there is only one item being displayed in the tab list.
  TriggerMouseEvent(first_row, ui::EventType::ET_MOUSE_ENTERED);
  EXPECT_FALSE(close_button->GetVisible());
  TriggerMouseEvent(first_row, ui::EventType::ET_MOUSE_EXITED);
  EXPECT_FALSE(close_button->GetVisible());
}
