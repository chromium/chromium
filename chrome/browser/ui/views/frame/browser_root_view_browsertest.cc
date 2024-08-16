// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_root_view.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"

class BrowserRootViewBrowserTest : public InProcessBrowserTest {
 public:
  BrowserRootViewBrowserTest() = default;

  BrowserRootViewBrowserTest(const BrowserRootViewBrowserTest&) = delete;
  BrowserRootViewBrowserTest& operator=(const BrowserRootViewBrowserTest&) =
      delete;

  BrowserRootView* browser_root_view() {
    BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
    return static_cast<BrowserRootView*>(
        browser_view->GetWidget()->GetRootView());
  }

  void PerformMouseWheelOnTabStrip(const gfx::Vector2d& offset) {
    TabStrip* tabstrip =
        BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
    const gfx::Point tabstrip_center = tabstrip->GetLocalBounds().CenterPoint();
    const gfx::Point location = views::View::ConvertPointToTarget(
        tabstrip, browser_root_view(), tabstrip_center);
    const gfx::Point root_location =
        views::View::ConvertPointToScreen(tabstrip, tabstrip_center);

    ui::MouseWheelEvent wheel_event(offset, location, root_location,
                                    ui::EventTimeForNow(), /*flags=*/0,
                                    /*changed_button_flags=*/0);
    browser_root_view()->OnMouseWheel(wheel_event);
  }

  void StartAndFinishDrag(const ui::OSExchangeData& data,
                          ui::mojom::DragOperation& out_drag_op) {
    ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                              ui::DragDropTypes::DRAG_COPY);
    BrowserRootView* root_view = browser_root_view();

    base::RunLoop run_loop;
    root_view->SetOnFilteringCompleteClosureForTesting(run_loop.QuitClosure());
    root_view->OnDragEntered(event);

    // At this point, the drag information will have been set, and a background
    // task will have been posted to process the dragged URLs
    // (`GetURLMimeTypes()` -> `FilterURLs()`). Ensure that all background
    // processing is complete before checking the drag operation or invoking the
    // drag callback.
    run_loop.Run();

    EXPECT_NE(ui::DragDropTypes::DRAG_NONE, root_view->OnDragUpdated(event));

    auto drop_cb = root_view->GetDropCallback(event);
    std::move(drop_cb).Run(event, out_drag_op,
                           /*drag_image_layer_owner=*/nullptr);
  }
};

// TODO(crbug.com/40186503): These tests produces wayland protocol error
// wl_display.error(xdg_surface, 1, "popup parent not constructed") on LaCrOS
// with Exo.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Clear drop info after performing drop. http://crbug.com/838791
IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, ClearDropInfo) {
  ui::OSExchangeData data;
  data.SetURL(GURL("http://www.chromium.org/"), std::u16string());

  auto* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->count(), 1);

  ui::mojom::DragOperation drag_op = ui::mojom::DragOperation::kNone;
  StartAndFinishDrag(data, drag_op);

  EXPECT_EQ(drag_op, ui::mojom::DragOperation::kCopy);
  EXPECT_EQ(tab_strip_model->count(), 2);
  EXPECT_FALSE(browser_root_view()->drop_info_);
}

// Make sure plain string is droppable. http://crbug.com/838794
IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, PlainString) {
  ui::OSExchangeData data;
  data.SetString(u"Plain string");

  ui::mojom::DragOperation drag_op = ui::mojom::DragOperation::kNone;
  StartAndFinishDrag(data, drag_op);

  EXPECT_NE(ui::mojom::DragOperation::kNone, drag_op);
}

// Clear drop target when the widget is being destroyed.
// http://crbug.com/1001942
IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, ClearDropTarget) {
  ui::OSExchangeData data;
  data.SetURL(GURL("http://www.chromium.org/"), std::u16string());
  ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_COPY);

  browser_root_view()->OnDragUpdated(event);

  // Calling this will cause segmentation fault if |root_view| doesn't clear
  // the target.
  CloseBrowserSynchronously(browser());
}

IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, OnDragEnteredNoTabs) {
  auto* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->count(), 1);
  EXPECT_EQ(tab_strip_model->active_index(), 0);
  tab_strip_model->CloseAllTabs();
  EXPECT_EQ(tab_strip_model->count(), 0);
  EXPECT_EQ(tab_strip_model->active_index(), -1);

  ui::OSExchangeData data;
  data.SetURL(GURL("file:///test.txt"), std::u16string());
  ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DRAG_COPY);

  // Ensure OnDragEntered() doesn't crash when there are no active tabs.
  browser_root_view()->OnDragEntered(event);
}

IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, WheelTabChange) {
  if (!browser_defaults::kScrollEventChangesTab) {
    GTEST_SKIP() << "Test does not apply to this platform.";
  }

  TabStripModel* model = browser()->tab_strip_model();

  while (model->count() < 2) {
    ASSERT_TRUE(
        AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_LINK));
  }
  model->ActivateTabAt(1);
  ASSERT_EQ(1, model->active_index());

  const gfx::Vector2d kWheelDown(0, -ui::MouseWheelEvent::kWheelDelta);
  const gfx::Vector2d kWheelUp(0, ui::MouseWheelEvent::kWheelDelta);

  // At the end of the tab strip, scrolling to the next tab does not loop around
  // to the beginning.
  PerformMouseWheelOnTabStrip(kWheelDown);
  EXPECT_EQ(1, model->active_index());

  PerformMouseWheelOnTabStrip(kWheelUp);
  EXPECT_EQ(0, model->active_index());

  // At the beginning of the tab strip, scrolling to the previous tab does not
  // loop around to the end.
  PerformMouseWheelOnTabStrip(kWheelUp);
  EXPECT_EQ(0, model->active_index());

  PerformMouseWheelOnTabStrip(kWheelDown);
  EXPECT_EQ(1, model->active_index());
}

IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest,
                       WheelTabChangeWithCollapsedTabGroups) {
  if (!browser_defaults::kScrollEventChangesTab) {
    GTEST_SKIP() << "Test does not apply to this platform.";
  }

  TabStripModel* model = browser()->tab_strip_model();
  TabStrip* tabstrip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  ASSERT_TRUE(model->SupportsTabGroups());

  // Create 5 tabs, with the leftmost, center, and rightmost in collapsed tab
  // groups.
  while (model->count() < 5) {
    ASSERT_TRUE(
        AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_LINK));
  }
  model->ActivateTabAt(1);
  tab_groups::TabGroupId left_group = model->AddToNewGroup({0});
  tab_groups::TabGroupId center_group = model->AddToNewGroup({2});
  tab_groups::TabGroupId right_group = model->AddToNewGroup({4});
  tabstrip->ToggleTabGroupCollapsedState(left_group);
  tabstrip->ToggleTabGroupCollapsedState(center_group);
  tabstrip->ToggleTabGroupCollapsedState(right_group);
  ASSERT_EQ(1, model->active_index());
  ASSERT_TRUE(model->IsTabCollapsed(0));
  ASSERT_FALSE(model->IsTabCollapsed(1));
  ASSERT_TRUE(model->IsTabCollapsed(2));
  ASSERT_FALSE(model->IsTabCollapsed(3));
  ASSERT_TRUE(model->IsTabCollapsed(4));

  const gfx::Vector2d kWheelDown(0, -ui::MouseWheelEvent::kWheelDelta);
  const gfx::Vector2d kWheelUp(0, ui::MouseWheelEvent::kWheelDelta);

  // Scrolling to the previous tab from index 1 should do nothing, since the
  // only previous tab is collapsed.
  PerformMouseWheelOnTabStrip(kWheelUp);
  EXPECT_EQ(1, model->active_index());

  // Scrolling to the next tab from index 1 should skip to index 3, since index
  // 2 in between them is collapsed.
  PerformMouseWheelOnTabStrip(kWheelDown);
  EXPECT_EQ(3, model->active_index());

  // Scrolling to the next tab from index 3 should do nothing, since the only
  // next tab is collapsed.
  PerformMouseWheelOnTabStrip(kWheelDown);
  EXPECT_EQ(3, model->active_index());
}

IN_PROC_BROWSER_TEST_F(BrowserRootViewBrowserTest, DropOrderingCorrect) {
  TabStripModel* model = browser()->tab_strip_model();

  // HELPER FUNCTION: Verify that the tabs in the current browser window match
  // the expected list of tabs.
  auto assert_tab_order = [model](std::vector<std::string_view> tab_urls,
                                  std::optional<int> active_tab) {
    ASSERT_EQ(static_cast<size_t>(model->count()), tab_urls.size());
    for (size_t i = 0; i < tab_urls.size(); ++i) {
      content::WebContents* contents = model->GetWebContentsAt(i);
      EXPECT_TRUE(content::WaitForLoadStop(contents));
      ASSERT_EQ(contents->GetURL(), GURL(tab_urls[i]));
    }
    if (active_tab.has_value()) {
      EXPECT_EQ(model->active_index(), active_tab.value());
    }
  };

  // HELPER FUNCTION: Reset to three tabs:
  // [about:blank?1][about:blank?2][about:blank?3]
  auto reset_tabs = [this, model, assert_tab_order]() {
    ASSERT_TRUE(
        AddTabAtIndex(0, GURL("about:blank?1"), ui::PAGE_TRANSITION_LINK));
    ASSERT_TRUE(
        AddTabAtIndex(1, GURL("about:blank?2"), ui::PAGE_TRANSITION_LINK));
    ASSERT_TRUE(
        AddTabAtIndex(2, GURL("about:blank?3"), ui::PAGE_TRANSITION_LINK));

    while (model->count() > 3) {
      model->DetachAndDeleteWebContentsAt(3);
    }

    assert_tab_order({"about:blank?1", "about:blank?2", "about:blank?3"},
                     std::nullopt);
  };

  // HELPER FUNCTION: Generate a DropInfo pointer.
  using BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex;
  using BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex;
  auto make_drop_info =
      [](std::vector<std::string_view> url_strings, int index,
         BrowserRootView::DropIndex::RelativeToIndex relative_to_index) {
        std::vector<GURL> urls;
        for (const auto& url_string : url_strings) {
          urls.push_back(GURL(url_string));
        }
        std::unique_ptr<BrowserRootView::DropInfo> drop_info;
        drop_info = std::make_unique<BrowserRootView::DropInfo>();
        drop_info->urls = std::move(urls);
        drop_info->index.emplace();
        drop_info->index->index = index;
        drop_info->index->relative_to_index = relative_to_index;
        return drop_info;
      };

  // A bunch of variables that are needed for the calls to NavigateToDroppedUrls
  // but that aren't tested here.
  OSExchangeData os_exchange_data;
  ui::DropTargetEvent event(os_exchange_data, gfx::PointF(), gfx::PointF(),
                            ui::DragDropTypes::DragOperation::DRAG_COPY);
  ui::mojom::DragOperation drag_operation;
  std::unique_ptr<BrowserRootView::DropInfo> drop_info;

  // Finally, the actual tests!

  {
    SCOPED_TRACE("Insert single URL on left");
    reset_tabs();
    drop_info = make_drop_info({"about:blank?A"}, 0, kInsertBeforeIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order(
        {"about:blank?A", "about:blank?1", "about:blank?2", "about:blank?3"},
        0);
  }
  {
    SCOPED_TRACE("Insert single URL between two tabs");
    reset_tabs();
    drop_info = make_drop_info({"about:blank?A"}, 1, kInsertBeforeIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order(
        {"about:blank?1", "about:blank?A", "about:blank?2", "about:blank?3"},
        1);
  }
  {
    SCOPED_TRACE("Insert single URL on right");
    reset_tabs();
    drop_info = make_drop_info({"about:blank?A"}, 3, kInsertBeforeIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order(
        {"about:blank?1", "about:blank?2", "about:blank?3", "about:blank?A"},
        3);
  }
  {
    SCOPED_TRACE("Insert multiple URLs on left");
    reset_tabs();
    drop_info =
        make_drop_info({"about:blank?A", "about:blank?B", "about:blank?C"}, 0,
                       kInsertBeforeIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order({"about:blank?A", "about:blank?B", "about:blank?C",
                      "about:blank?1", "about:blank?2", "about:blank?3"},
                     0);
  }
  {
    SCOPED_TRACE("Insert multiple URLs between two tabs");
    reset_tabs();
    drop_info =
        make_drop_info({"about:blank?A", "about:blank?B", "about:blank?C"}, 1,
                       kInsertBeforeIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order({"about:blank?1", "about:blank?A", "about:blank?B",
                      "about:blank?C", "about:blank?2", "about:blank?3"},
                     1);
  }
  {
    SCOPED_TRACE("Insert multiple URLs on right");
    reset_tabs();
    drop_info =
        make_drop_info({"about:blank?A", "about:blank?B", "about:blank?C"}, 3,
                       kInsertBeforeIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order({"about:blank?1", "about:blank?2", "about:blank?3",
                      "about:blank?A", "about:blank?B", "about:blank?C"},
                     3);
  }
  {
    SCOPED_TRACE("Replace single URL on left");
    reset_tabs();
    drop_info = make_drop_info({"about:blank?A"}, 0, kReplaceIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order({"about:blank?A", "about:blank?2", "about:blank?3"}, 0);
  }
  {
    SCOPED_TRACE("Replace single URL between two tabs");
    reset_tabs();
    drop_info = make_drop_info({"about:blank?A"}, 1, kReplaceIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order({"about:blank?1", "about:blank?A", "about:blank?3"}, 1);
  }
  {
    SCOPED_TRACE("Replace single URL on right");
    reset_tabs();
    drop_info = make_drop_info({"about:blank?A"}, 2, kReplaceIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order({"about:blank?1", "about:blank?2", "about:blank?A"}, 2);
  }
  {
    SCOPED_TRACE("Replace multiple URLs on left");
    reset_tabs();
    drop_info = make_drop_info(
        {"about:blank?A", "about:blank?B", "about:blank?C"}, 0, kReplaceIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order({"about:blank?A", "about:blank?B", "about:blank?C",
                      "about:blank?2", "about:blank?3"},
                     0);
  }
  {
    SCOPED_TRACE("Replace multiple URLs between two tabs");
    reset_tabs();
    drop_info = make_drop_info(
        {"about:blank?A", "about:blank?B", "about:blank?C"}, 1, kReplaceIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order({"about:blank?1", "about:blank?A", "about:blank?B",
                      "about:blank?C", "about:blank?3"},
                     1);
  }
  {
    SCOPED_TRACE("Replace multiple URLs on right");
    reset_tabs();
    drop_info = make_drop_info(
        {"about:blank?A", "about:blank?B", "about:blank?C"}, 2, kReplaceIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order({"about:blank?1", "about:blank?2", "about:blank?A",
                      "about:blank?B", "about:blank?C"},
                     2);
  }
  {
    SCOPED_TRACE("Error case: no index");
    reset_tabs();
    drop_info = make_drop_info({"about:blank?A"}, 0, kInsertBeforeIndex);
    drop_info->index.reset();
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order({"about:blank?1", "about:blank?2", "about:blank?3"},
                     std::nullopt);
  }
  {
    SCOPED_TRACE("Error case: index too large");
    reset_tabs();
    drop_info = make_drop_info({"about:blank?A"}, 10, kInsertBeforeIndex);
    browser_root_view()->NavigateToDroppedUrls(std::move(drop_info), event,
                                               drag_operation, nullptr);
    assert_tab_order({"about:blank?1", "about:blank?2", "about:blank?3"},
                     std::nullopt);
  }
}

#endif  // #if !BUILDFLAG(IS_CHROMEOS_LACROS)
