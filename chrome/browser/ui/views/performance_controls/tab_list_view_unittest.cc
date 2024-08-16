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
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
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

  std::unique_ptr<views::Widget> CreateWidget() {
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.context = GetContext();
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget->Init(std::move(params));
    widget->Show();
    return widget;
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

  TabListRowView* const second_row =
      views::AsViewClass<TabListRowView>(children[1]);
  EXPECT_EQ(second_row->GetTitleTextForTesting(), u"a.com");
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
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(close_button);
  test_api.NotifyClick(e);

  EXPECT_EQ(tab_list_view->children().size(), 1u);
  EXPECT_EQ(tab_list_model->page_contexts().size(), 1u);
}

TEST_F(TabListViewUnitTest, CloseButtonShowsAndHidesUpdate) {
  AddTab(browser(), GURL("https://a.com"));
  AddTab(browser(), GURL("https://b.com"));

  auto tab_list_model =
      std::make_unique<TabListModel>(GetPageContextAtIndices({0, 1}));
  auto widget = CreateWidget();
  TabListView* const tab_list_view = widget->SetContentsView(
      std::make_unique<TabListView>(tab_list_model.get()));
  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      GetContext(), widget->GetNativeWindow());

  std::vector<resource_attribution::PageContext> page_contexts =
      tab_list_model->page_contexts();
  auto children = tab_list_view->children();
  ASSERT_EQ(children.size(), 2u);

  TabListRowView* const first_row =
      views::AsViewClass<TabListRowView>(children[0]);
  views::ImageButton* const close_button =
      first_row->GetCloseButtonForTesting();

  EXPECT_FALSE(close_button->GetVisible());

  event_generator->MoveMouseTo(first_row->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(first_row->IsMouseHovered());
  EXPECT_TRUE(close_button->GetVisible());

  TabListRowView* const second_row =
      views::AsViewClass<TabListRowView>(children[1]);
  views::ImageButton* const second_close_button =
      second_row->GetCloseButtonForTesting();

  event_generator->MoveMouseTo(second_row->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(first_row->IsMouseHovered());
  EXPECT_FALSE(close_button->GetVisible());
  EXPECT_TRUE(second_close_button->GetVisible());

  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(second_close_button);
  test_api.NotifyClick(e);

  // The close button should now stay hidden regardless of the mouse movement
  // since there is only one item being displayed in the tab list.
  event_generator->MoveMouseTo(first_row->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(first_row->IsMouseHovered());
  EXPECT_FALSE(close_button->GetVisible());
}

TEST_F(TabListViewUnitTest, CloseButtonShowsAndHidesWithFocus) {
  AddTab(browser(), GURL("https://a.com"));
  AddTab(browser(), GURL("https://b.com"));

  auto tab_list_model =
      std::make_unique<TabListModel>(GetPageContextAtIndices({0, 1}));
  auto widget = CreateWidget();
  TabListView* const tab_list_view = widget->SetContentsView(
      std::make_unique<TabListView>(tab_list_model.get()));
  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      GetContext(), widget->GetNativeWindow());

  std::vector<resource_attribution::PageContext> page_contexts =
      tab_list_model->page_contexts();
  auto children = tab_list_view->children();
  ASSERT_EQ(children.size(), 2u);

  TabListRowView* const first_row =
      views::AsViewClass<TabListRowView>(children[0]);
  views::ImageButton* const close_button =
      first_row->GetCloseButtonForTesting();

  EXPECT_FALSE(close_button->GetVisible());

  event_generator->MoveMouseTo(first_row->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(first_row->IsMouseHovered());
  EXPECT_TRUE(close_button->GetVisible());

  close_button->RequestFocus();
  EXPECT_TRUE(close_button->HasFocus());

  TabListRowView* const second_row =
      views::AsViewClass<TabListRowView>(children[1]);
  views::ImageButton* const second_close_button =
      second_row->GetCloseButtonForTesting();

  // Move the mouse to hover over the second row but keep the focus on the first
  // row. The close buttons for both rows should show since they are either
  // being focused or have the mouse hovering over the row.
  event_generator->MoveMouseTo(second_row->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(first_row->IsMouseHovered());
  EXPECT_TRUE(close_button->GetVisible());
  EXPECT_TRUE(second_close_button->GetVisible());

  // Remove the second row from the suggested tab list. The close button for the
  // first row should be hidden at this point because there is only one row left
  // in the list and a single item in the list is not removable.
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(second_close_button);
  test_api.NotifyClick(e);
  EXPECT_FALSE(close_button->GetVisible());
}

TEST_F(TabListViewUnitTest, AccessibleProperties) {
  AddTab(browser(), GURL("https://a.com"));

  // TabListView accessible properties test.
  auto tab_list_model =
      std::make_unique<TabListModel>(GetPageContextAtIndices({0}));
  auto tab_list_view = std::make_unique<TabListView>(tab_list_model.get());
  ui::AXNodeData data;

  tab_list_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kListBox);

  // TabListRowView text container accessible properties test.
  TabListRowView* const row =
      views::AsViewClass<TabListRowView>(tab_list_view->children()[0]);
  auto* const text_container = row->GetTextContainerForTesting();

  data = ui::AXNodeData();
  text_container->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kListBoxOption);
}
