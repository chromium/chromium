// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view.h"

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/test/split_view_browser_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/split_tab_data.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/controls/separator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using testing::Return;
using testing::ReturnRef;

namespace {

class MockDragController : public TabDragDelegate::DragController {
 public:
  MockDragController() = default;
  MockDragController(const MockDragController&) = delete;
  MockDragController& operator=(const MockDragController&) = delete;
  ~MockDragController() override = default;

  MOCK_METHOD(std::unique_ptr<tabs::TabModel>,
              DetachTabAtForInsertion,
              (int),
              (override));
  MOCK_METHOD(const DragSessionData&, GetSessionData, (), (const, override));
};

}  // namespace

class MultiContentsViewBrowserTest
    : public SplitViewBrowserTestMixin<InProcessBrowserTest> {};

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleDropTargetViewLinkDrop_IsSupported) {
// TODO(crbug.com/425715421): Fix drag and drop on Wayland.
#if BUILDFLAG(IS_OZONE)
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_split_view_drag_and_drop) {
    return;
  }
#endif

  EXPECT_TRUE(multi_contents_view()->IsDragAndDropEnabled());

  Browser::CreateParams app_browser_params =
      Browser::CreateParams::CreateForApp("AppName", true, gfx::Rect(),
                                          browser()->profile(), false);
  Browser* app_browser = Browser::Create(app_browser_params);

  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(app_browser)
                   ->multi_contents_view()
                   ->IsDragAndDropEnabled());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleDropTargetViewLinkDrop_EndDropTarget) {
// TODO(crbug.com/425715421): Fix drag and drop on Wayland.
#if BUILDFLAG(IS_OZONE)
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_split_view_drag_and_drop) {
    return;
  }
#endif

  ui::OSExchangeData data;
  const GURL kDropUrl("http://www.chromium.org/");
  data.SetURL(kDropUrl, u"Chromium");
  gfx::PointF point = {10, 10};
  ui::DropTargetEvent event(data, point, point, ui::DragDropTypes::DRAG_LINK);

  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::END,
                           MultiContentsDropTargetView::DropTargetState::kFull);
  auto drop_cb = drop_target_view()->GetDropCallback(event);
  EXPECT_FALSE(multi_contents_view()->IsInSplitView());

  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_TRUE(multi_contents_view()->IsInSplitView());

  // After the drop, a new tab should be created in the split view.
  // The original tab is at index 0, the new tab from the drop is at index 1.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(kDropUrl,
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleDropTargetViewLinkDrop_StartDropTarget) {
  // TODO(crbug.com/425715421): Fix drag and drop on Wayland.
#if BUILDFLAG(IS_OZONE)
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_split_view_drag_and_drop) {
    return;
  }
#endif
  ui::OSExchangeData data;
  const GURL kDropUrl("http://www.chromium.org/");
  data.SetURL(kDropUrl, u"Chromium");
  gfx::PointF point = {10, 10};
  ui::DropTargetEvent event(data, point, point, ui::DragDropTypes::DRAG_LINK);

  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::START,
                           MultiContentsDropTargetView::DropTargetState::kFull);
  auto drop_cb = drop_target_view()->GetDropCallback(event);
  EXPECT_FALSE(multi_contents_view()->IsInSplitView());

  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_TRUE(multi_contents_view()->IsInSplitView());

  // After the drop, a new tab should be created in the split view.
  // The original tab is at index 0, the new tab from the drop is at index 1.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(kDropUrl,
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleDropTargetViewLinkDrop_BlockJavascriptUrl) {
  // TODO(crbug.com/425715421): Fix drag and drop on Wayland.
#if BUILDFLAG(IS_OZONE)
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_split_view_drag_and_drop) {
    return;
  }
#endif
  ui::OSExchangeData data;
  const GURL kDropUrl("javascript:alert(1)");
  data.SetURL(kDropUrl, u"javascript");
  gfx::PointF point = {10, 10};
  ui::DropTargetEvent event(data, point, point, ui::DragDropTypes::DRAG_LINK);

  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::START,
                           MultiContentsDropTargetView::DropTargetState::kFull);
  auto drop_cb = drop_target_view()->GetDropCallback(event);
  EXPECT_FALSE(multi_contents_view()->IsInSplitView());

  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_TRUE(multi_contents_view()->IsInSplitView());

  // After the drop, a new tab should be created in the split view.
  // The original tab is at index 0, the new tab from the drop is at index 1.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GURL(content::kBlockedURL),
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleTabDrop_EndDropTarget) {
  // TODO(crbug.com/425715421): Fix drag and drop on Wayland.
#if BUILDFLAG(IS_OZONE)
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_split_view_drag_and_drop) {
    return;
  }
#endif

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->count());
  EXPECT_FALSE(multi_contents_view()->IsInSplitView());

  // Show the drop target on the end side.
  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::END,
                           MultiContentsDropTargetView::DropTargetState::kFull);

  // Create a second browser with a tab to be dragged.
  Browser* browser2 = CreateBrowser(browser()->profile());
  content::WebContents* contents_to_drop =
      browser2->GetTabStripModel()->GetActiveWebContents();

  // Mock the drag controller to simulate a tab drop.
  MockDragController controller;
  DragSessionData session_data;
  session_data.source_view_index_ = 0;
  EXPECT_CALL(controller, GetSessionData).WillOnce(ReturnRef(session_data));
  EXPECT_CALL(controller, DetachTabAtForInsertion(0))
      .WillOnce(
          Return(browser2->GetTabStripModel()->DetachTabAtForInsertion(0)));

  // Handle the tab drop.
  multi_contents_view()->drop_target_controller().HandleTabDrop(controller);

  // Verify the state after the drop.
  EXPECT_TRUE(multi_contents_view()->IsInSplitView());
  ASSERT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(contents_to_drop, tab_strip_model->GetWebContentsAt(1));
  EXPECT_EQ(1, tab_strip_model->active_index());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleTabDrop_StartDropTarget) {
  // TODO(crbug.com/425715421): Fix drag and drop on Wayland.
#if BUILDFLAG(IS_OZONE)
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_split_view_drag_and_drop) {
    return;
  }
#endif

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* original_contents =
      tab_strip_model->GetActiveWebContents();
  ASSERT_EQ(1, tab_strip_model->count());
  EXPECT_FALSE(multi_contents_view()->IsInSplitView());

  // Show the drop target on the start side.
  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::START,
                           MultiContentsDropTargetView::DropTargetState::kFull);

  // Create a second browser with a tab to be dragged.
  Browser* browser2 = CreateBrowser(browser()->profile());
  content::WebContents* contents_to_drop =
      browser2->GetTabStripModel()->GetActiveWebContents();

  // Mock the drag controller to simulate a tab drop.
  MockDragController controller;
  DragSessionData session_data;
  session_data.source_view_index_ = 0;
  EXPECT_CALL(controller, GetSessionData).WillOnce(ReturnRef(session_data));
  EXPECT_CALL(controller, DetachTabAtForInsertion(0))
      .WillOnce(
          Return(browser2->GetTabStripModel()->DetachTabAtForInsertion(0)));

  // Handle the tab drop.
  multi_contents_view()->drop_target_controller().HandleTabDrop(controller);

  // Verify the state after the drop.
  EXPECT_TRUE(multi_contents_view()->IsInSplitView());
  ASSERT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(contents_to_drop, tab_strip_model->GetWebContentsAt(0));
  EXPECT_EQ(original_contents, tab_strip_model->GetWebContentsAt(1));
  EXPECT_EQ(0, tab_strip_model->active_index());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest, DragAndDropEnabledPref) {
// TODO(crbug.com/425715421): Fix drag and drop on Wayland.
#if BUILDFLAG(IS_OZONE)
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_split_view_drag_and_drop) {
    return;
  }
#endif

  // Drag and drop should be enabled by default.
  EXPECT_TRUE(multi_contents_view()->IsDragAndDropEnabled());

  // Disable drag and drop.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSplitViewDragAndDropEnabled, false);
  EXPECT_FALSE(multi_contents_view()->IsDragAndDropEnabled());

  // Enable drag and drop.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSplitViewDragAndDropEnabled, true);
  EXPECT_TRUE(multi_contents_view()->IsDragAndDropEnabled());
}

// Test class for WebContents ReLayout.
class MultiContentsViewWebContentsReLayoutBrowserTest
    : public SplitViewBrowserTestMixin<InProcessBrowserTest> {
 protected:

  static constexpr char kReLayoutTestURL[] = "/re_layout_test.html";

  void SetUpOnMainThread() override {
    CreateTestServer(base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
    EXPECT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
  }

  void CheckNoResizeHappened() {
    auto* tab_strip_model = browser()->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); i++) {
      auto* web_contents = tab_strip_model->GetWebContentsAt(i);
      EXPECT_TRUE(content::WaitForLoadStop(web_contents));
      EXPECT_EQ(false, content::EvalJs(web_contents, "window.has_resized"));
    }
  }

  int GetResizeCount(content::WebContents* web_contents) {
    return content::EvalJs(web_contents, "window.resize_count").ExtractInt();
  }

  void CreateSplitTabAndLoadReLayoutTestPage() {
    CreateSplitView();
    LoadReLayoutTestPageInActiveSplitTabs();
  }

  void CreateSplitView() {
    auto* tab_strip_model = browser()->tab_strip_model();
    const int active_index = tab_strip_model->active_index();

    RunScheduledLayouts();
    chrome::NewSplitTab(browser(),
                        split_tabs::SplitTabCreatedSource::kToolbarButton);
    EXPECT_TRUE(content::WaitForLoadStop(
        tab_strip_model->GetWebContentsAt(active_index + 1)));
    RunScheduledLayouts();
  }

  void LoadReLayoutTestPageInActiveSplitTabs() {
    auto* tab_strip_model = browser()->tab_strip_model();
    const int active_index = tab_strip_model->active_index();
    split_tabs::SplitTabId split_id =
        tab_strip_model->GetSplitForTab(active_index).value();
    split_tabs::SplitTabData* split_data =
        tab_strip_model->GetSplitData(split_id);
    ASSERT_TRUE(split_data);

    const GURL test_url = embedded_test_server()->GetURL(kReLayoutTestURL);
    for (tabs::TabInterface* tab : split_data->ListTabs()) {
      tab->GetContents()->GetController().LoadURL(test_url, content::Referrer(),
                                                  ui::PAGE_TRANSITION_TYPED,
                                                  std::string());
      EXPECT_TRUE(content::WaitForLoadStop(tab->GetContents()));
    }
  }
};

// TODO(https://crbug.com/430525043): Flaky on Linux.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SwitchingTabsShouldNotTriggerWebContentsReLayout_SplitNoSplit \
  DISABLED_SwitchingTabsShouldNotTriggerWebContentsReLayout_SplitNoSplit
#else
#define MAYBE_SwitchingTabsShouldNotTriggerWebContentsReLayout_SplitNoSplit \
  SwitchingTabsShouldNotTriggerWebContentsReLayout_SplitNoSplit
#endif
IN_PROC_BROWSER_TEST_F(
    MultiContentsViewWebContentsReLayoutBrowserTest,
    MAYBE_SwitchingTabsShouldNotTriggerWebContentsReLayout_SplitNoSplit) {
  auto* tab_strip_model = browser()->tab_strip_model();

  const GURL test_url = embedded_test_server()->GetURL(kReLayoutTestURL);

  // Load the test page in the active tab.
  tab_strip_model->GetActiveWebContents()->GetController().LoadURL(
      test_url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(
      content::WaitForLoadStop(tab_strip_model->GetActiveWebContents()));

  // Add a new tab and open split view.
  EXPECT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  CreateSplitTabAndLoadReLayoutTestPage();

  // Focus on the split tab.
  tab_strip_model->GetWebContentsAt(1)->Focus();
  RunScheduledLayouts();

  // Switching tabs should not trigger a re-layout.
  tab_strip_model->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  RunScheduledLayouts();
  tab_strip_model->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  RunScheduledLayouts();

  // No resize should have happened in the web contents.
  CheckNoResizeHappened();
}

IN_PROC_BROWSER_TEST_F(
    MultiContentsViewWebContentsReLayoutBrowserTest,
    SwitchingTabsShouldNotTriggerWebContentsReLayout_SplitSplit) {
  auto* tab_strip_model = browser()->tab_strip_model();

  const GURL test_url = embedded_test_server()->GetURL(kReLayoutTestURL);

  // Open split view and test page.
  CreateSplitTabAndLoadReLayoutTestPage();

  // Focus on the split tab.
  tab_strip_model->GetWebContentsAt(1)->Focus();

  // Add a new tab and open split view.
  EXPECT_TRUE(
      AddTabAtIndex(2, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  CreateSplitView();

  // Change the size.
  multi_contents_view()->OnResize(multi_contents_view()->width() * 0.3, true);
  RunScheduledLayouts();

  // Load the test page in the active tab and split tab.
  LoadReLayoutTestPageInActiveSplitTabs();
  RunScheduledLayouts();

  // Switching tabs should not trigger a re-layout.
  tab_strip_model->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  RunScheduledLayouts();
  tab_strip_model->ActivateTabAt(
      2, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  RunScheduledLayouts();

  // No resize should have happened in the web contents.
  CheckNoResizeHappened();
}

// TODO(crbug.com/429495554): Flaky on Mac and Windows.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_EnterAndExitFullscreenInSplitTabShouldOnlyResizeTwice \
  DISABLED_EnterAndExitFullscreenInSplitTabShouldOnlyResizeTwice
#else
#define MAYBE_EnterAndExitFullscreenInSplitTabShouldOnlyResizeTwice \
  EnterAndExitFullscreenInSplitTabShouldOnlyResizeTwice
#endif
IN_PROC_BROWSER_TEST_F(
    MultiContentsViewWebContentsReLayoutBrowserTest,
    MAYBE_EnterAndExitFullscreenInSplitTabShouldOnlyResizeTwice) {
#if BUILDFLAG(IS_OZONE)
  // TODO(crbug.com/429495554): Investigate why this test failed on wayland.
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland") {
    GTEST_SKIP();
  }
#endif
  auto* tab_strip_model = browser()->tab_strip_model();

  const GURL test_url = embedded_test_server()->GetURL(kReLayoutTestURL);

  CreateSplitView();

  // Change the size.
  multi_contents_view()->OnResize(multi_contents_view()->width() * 0.3, true);
  RunScheduledLayouts();

  // Load the test page in the active tab and split tab.
  LoadReLayoutTestPageInActiveSplitTabs();
  RunScheduledLayouts();

  // Focus on the split tab.
  tab_strip_model->GetWebContentsAt(1)->Focus();
  RunScheduledLayouts();

  // Enter fullscreen in the split tab.
  content::WebContents* split_tab = tab_strip_model->GetWebContentsAt(1);
  split_tab->GetDelegate()->EnterFullscreenModeForTab(
      split_tab->GetPrimaryMainFrame(), {});
  ui_test_utils::FullscreenWaiter(browser(), {.tab_fullscreen = true}).Wait();
  RunScheduledLayouts();

  EXPECT_TRUE(base::test::RunUntil(
      [this, split_tab]() { return GetResizeCount(split_tab) >= 1; }));

  // Exit fullscreen in the split tab.
  split_tab->GetDelegate()->ExitFullscreenModeForTab(split_tab);
  ui_test_utils::FullscreenWaiter(
      browser(), ui_test_utils::FullscreenWaiter::kNoFullscreen)
      .Wait();
  RunScheduledLayouts();

  EXPECT_TRUE(base::test::RunUntil(
      [this, split_tab]() { return GetResizeCount(split_tab) >= 2; }));
  RunScheduledLayouts();

  // Should resized twice.
  EXPECT_EQ(GetResizeCount(split_tab), 2);
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest, SeparatorLayout) {
  MultiContentsView* view = multi_contents_view();
  view->SetShouldShowTrailingSeparator(true);
  view->SetShouldShowLeadingSeparator(true);
  view->SetShouldShowTopSeparator(true);

  gfx::Rect initial_bounds(10, 20, 100, 80);
  std::vector<views::ChildLayout> actual_child_layouts;

  gfx::Rect remaining_space =
      view->CalculateSeparatorLayouts(initial_bounds, actual_child_layouts);

  constexpr int kSeparatorThickness = views::Separator::kThickness;

  gfx::Rect expected_remaining_space(
      initial_bounds.x() + kSeparatorThickness,
      initial_bounds.y() + kSeparatorThickness,
      initial_bounds.width() - 2 * kSeparatorThickness,
      initial_bounds.height() - kSeparatorThickness);
  EXPECT_EQ(expected_remaining_space, remaining_space);

  std::vector<views::ChildLayout> expected_separator_layouts;
  expected_separator_layouts.emplace_back(
      view->contents_separators_.top_separator.get(), true,
      gfx::Rect(10, 20, 100, kSeparatorThickness));
  expected_separator_layouts.emplace_back(
      view->contents_separators_.leading_separator.get(), true,
      gfx::Rect(10, 20, kSeparatorThickness, 80));
  expected_separator_layouts.emplace_back(
      view->contents_separators_.trailing_separator.get(), true,
      gfx::Rect(10 + 100 - kSeparatorThickness, 20, kSeparatorThickness, 80));
  expected_separator_layouts.emplace_back(
      view->contents_separators_.top_leading_rounded_corner.get(), true,
      gfx::Rect(initial_bounds.origin(),
                view->contents_separators_.top_leading_rounded_corner
                    ->GetPreferredSize()));
  expected_separator_layouts.emplace_back(
      view->contents_separators_.top_trailing_rounded_corner.get(), true,
      gfx::Rect(
          gfx::Point(initial_bounds.right() -
                         view->contents_separators_.top_trailing_rounded_corner
                             ->GetPreferredSize()
                             .width(),
                     initial_bounds.y()),
          view->contents_separators_.top_trailing_rounded_corner
              ->GetPreferredSize()));

  EXPECT_THAT(actual_child_layouts,
              testing::UnorderedElementsAreArray(expected_separator_layouts));
}
