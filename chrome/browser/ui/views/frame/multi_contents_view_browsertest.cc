// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view.h"

#include <vector>

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/contents_separator.h"
#include "chrome/browser/ui/views/frame/custom_floating_corner.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_delegate.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "chrome/browser/ui/views/test/split_view_browser_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/split_tab_data.h"
#include "content/public/browser/navigation_controller.h"
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
#include "ui/base/page_transition_types.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/separator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

using testing::Return;
using testing::ReturnRef;

namespace {

class MockDragController : public TabDragTarget::DragController {
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
  MOCK_METHOD(const TabDragContext*, GetAttachedContext, (), (const, override));
};

void CompareLayouts(const views::ProposedLayout::ChildLayoutList& expected,
                    const views::ProposedLayout::ChildLayoutList& actual) {
  EXPECT_EQ(actual.size(), expected.size());
  for (const auto& expected_child : expected) {
    bool found = false;
    for (const auto& actual_child : actual) {
      if (expected_child.child_view == actual_child.child_view) {
        found = true;
        EXPECT_EQ(expected_child, actual_child)
            << "Expected layout " << actual_child.ToString() << " to equal "
            << expected_child.ToString();
        break;
      }
    }
    EXPECT_TRUE(found) << "Expected to find layout for "
                       << expected_child.child_view->GetClassName();
  }
}

}  // namespace

class MultiContentsViewBrowserTest
    : public SplitViewBrowserTestMixin<InProcessBrowserTest> {};

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleDropTargetViewLinkDrop_IsSupported) {
  EXPECT_TRUE(multi_contents_view()->IsDragAndDropEnabled());

  BrowserWindowCreateParams app_browser_params =
      BrowserWindowCreateParams::CreateForApp(
          "AppName",
          /*trusted_source=*/true, gfx::Rect(), browser()->GetProfile(),
          /*user_gesture=*/false);
  BrowserWindowInterface* app_browser =
      CreateBrowserWindow(std::move(app_browser_params));

  EXPECT_FALSE(BrowserView::GetBrowserViewForBrowser(app_browser)
                   ->multi_contents_view()
                   ->IsDragAndDropEnabled());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleDropTargetViewLinkDrop_EndDropTarget) {
  ui::OSExchangeData data;
  const GURL kDropUrl("http://www.chromium.org/");
  data.SetURL(kDropUrl, u"Chromium");
  gfx::PointF point = {10, 10};
  ui::DropTargetEvent event(data, point, point, ui::DragDropTypes::DRAG_LINK);

  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::END,
                           MultiContentsDropTargetView::DropTargetState::kFull,
                           MultiContentsDropTargetView::DragType::kLink);
  auto drop_cb = drop_target_view()->GetDropCallback(event);
  EXPECT_FALSE(multi_contents_view()->IsInSplitView());

  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_TRUE(multi_contents_view()->IsInSplitView());

  // After the drop, a new tab should be created in the split view.
  // The original tab is at index 0, the new tab from the drop is at index 1.
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->GetTabStripModel()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(kDropUrl,
            browser()->GetTabStripModel()->GetWebContentsAt(1)->GetURL());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleDropTargetViewLinkDrop_StartDropTarget) {
  ui::OSExchangeData data;
  const GURL kDropUrl("http://www.chromium.org/");
  data.SetURL(kDropUrl, u"Chromium");
  gfx::PointF point = {10, 10};
  ui::DropTargetEvent event(data, point, point, ui::DragDropTypes::DRAG_LINK);

  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::START,
                           MultiContentsDropTargetView::DropTargetState::kFull,
                           MultiContentsDropTargetView::DragType::kLink);
  auto drop_cb = drop_target_view()->GetDropCallback(event);
  EXPECT_FALSE(multi_contents_view()->IsInSplitView());

  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_TRUE(multi_contents_view()->IsInSplitView());

  // After the drop, a new tab should be created in the split view.
  // The original tab is at index 0, the new tab from the drop is at index 1.
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());
  EXPECT_EQ(kDropUrl,
            browser()->GetTabStripModel()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->GetTabStripModel()->GetWebContentsAt(1)->GetURL());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleDropTargetViewLinkDrop_PinnedWithStartDropTarget) {
  browser()->GetTabStripModel()->SetTabPinned(0, true);

  ui::OSExchangeData data;
  const GURL kDropUrl("http://www.chromium.org/");
  data.SetURL(kDropUrl, u"Chromium");
  gfx::PointF point = {10, 10};
  ui::DropTargetEvent event(data, point, point, ui::DragDropTypes::DRAG_LINK);

  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::START,
                           MultiContentsDropTargetView::DropTargetState::kFull,
                           MultiContentsDropTargetView::DragType::kLink);
  auto drop_cb = drop_target_view()->GetDropCallback(event);
  EXPECT_FALSE(multi_contents_view()->IsInSplitView());

  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_TRUE(multi_contents_view()->IsInSplitView());

  // After the drop, a new tab should be created in the split view. The original
  // tab is at index 0, the new tab from the drop is at index 1. Both tabs
  // should be pinned.
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());
  EXPECT_EQ(kDropUrl,
            browser()->GetTabStripModel()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->GetTabStripModel()->GetWebContentsAt(1)->GetURL());
  EXPECT_TRUE(browser()->GetTabStripModel()->GetTabAtIndex(0)->IsPinned());
  EXPECT_TRUE(browser()->GetTabStripModel()->GetTabAtIndex(1)->IsPinned());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleDropTargetViewLinkDrop_GroupedWithEndDropTarget) {
  browser()->GetTabStripModel()->AddToNewGroup({0});

  ui::OSExchangeData data;
  const GURL kDropUrl("http://www.chromium.org/");
  data.SetURL(kDropUrl, u"Chromium");
  gfx::PointF point = {10, 10};
  ui::DropTargetEvent event(data, point, point, ui::DragDropTypes::DRAG_LINK);

  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::END,
                           MultiContentsDropTargetView::DropTargetState::kFull,
                           MultiContentsDropTargetView::DragType::kLink);
  auto drop_cb = drop_target_view()->GetDropCallback(event);
  EXPECT_FALSE(multi_contents_view()->IsInSplitView());

  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_TRUE(multi_contents_view()->IsInSplitView());

  // After the drop, a new tab should be created in the split view. The original
  // tab is at index 0, the new tab from the drop is at index 1. Both tabs
  // should be in a group.
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->GetTabStripModel()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(kDropUrl,
            browser()->GetTabStripModel()->GetWebContentsAt(1)->GetURL());
  EXPECT_TRUE(
      browser()->GetTabStripModel()->GetTabAtIndex(0)->GetGroup().has_value());
  EXPECT_TRUE(
      browser()->GetTabStripModel()->GetTabAtIndex(1)->GetGroup().has_value());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleDropTargetViewLinkDrop_BlockJavascriptUrl) {
  ui::OSExchangeData data;
  const GURL kDropUrl("javascript:alert(1)");
  data.SetURL(kDropUrl, u"javascript");
  gfx::PointF point = {10, 10};
  ui::DropTargetEvent event(data, point, point, ui::DragDropTypes::DRAG_LINK);

  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::START,
                           MultiContentsDropTargetView::DropTargetState::kFull,
                           MultiContentsDropTargetView::DragType::kLink);
  auto drop_cb = drop_target_view()->GetDropCallback(event);
  EXPECT_FALSE(multi_contents_view()->IsInSplitView());

  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);

  EXPECT_TRUE(multi_contents_view()->IsInSplitView());

  // After the drop, a new tab should be created in the split view.
  // The original tab is at index 0, the new tab from the drop is at index 1.
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());
  EXPECT_EQ(GURL(content::kBlockedURL),
            browser()->GetTabStripModel()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            browser()->GetTabStripModel()->GetWebContentsAt(1)->GetURL());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       HandleTabDrop_EndDropTarget) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_EQ(1, tab_strip_model->count());
  EXPECT_FALSE(multi_contents_view()->IsInSplitView());

  // Show the drop target on the end side.
  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::END,
                           MultiContentsDropTargetView::DropTargetState::kFull,
                           MultiContentsDropTargetView::DragType::kLink);

  // Create a second browser with a tab to be dragged.
  BrowserWindowInterface* browser2 = CreateBrowser(browser()->GetProfile());
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
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  content::WebContents* original_contents =
      tab_strip_model->GetActiveWebContents();
  ASSERT_EQ(1, tab_strip_model->count());
  EXPECT_FALSE(multi_contents_view()->IsInSplitView());

  // Show the drop target on the start side.
  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::START,
                           MultiContentsDropTargetView::DropTargetState::kFull,
                           MultiContentsDropTargetView::DragType::kLink);

  // Create a second browser with a tab to be dragged.
  BrowserWindowInterface* browser2 = CreateBrowser(browser()->GetProfile());
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
  // Drag and drop should be enabled by default.
  EXPECT_TRUE(multi_contents_view()->IsDragAndDropEnabled());

  // Disable drag and drop.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSplitViewDragAndDropEnabled, false);
  EXPECT_FALSE(multi_contents_view()->IsDragAndDropEnabled());

  // Enable drag and drop.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kSplitViewDragAndDropEnabled, true);
  EXPECT_TRUE(multi_contents_view()->IsDragAndDropEnabled());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest,
                       DragAndDropDisabledForChromePages) {
  // Drag and drop should be enabled for normal pages.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  EXPECT_TRUE(multi_contents_view()->IsDragAndDropEnabled());

  // Drag and drop should be disabled for chrome:// pages.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));
  EXPECT_FALSE(multi_contents_view()->IsDragAndDropEnabled());

  // Drag and drop should be enabled for chrome://newtab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           chrome::ChromeUINewTabURLAsGURL()));
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
    auto* tab_strip_model = browser()->GetTabStripModel();
    const GURL test_url = embedded_test_server()->GetURL(kReLayoutTestURL);
    for (int i = 0; i < tab_strip_model->count(); i++) {
      auto* web_contents = tab_strip_model->GetWebContentsAt(i);
      EXPECT_TRUE(content::WaitForLoadStop(web_contents));
      if (web_contents->GetLastCommittedURL() != test_url) {
        continue;
      }
      EXPECT_EQ(false, content::EvalJs(web_contents, "window.has_resized"));
    }
  }

  std::vector<gfx::Size> GetResizeSizes(content::WebContents* web_contents) {
    const content::EvalJsResult result =
        content::EvalJs(web_contents, "window.resize_sizes");
    std::vector<gfx::Size> sizes;
    for (const base::Value& entry : result.ExtractList()) {
      const base::ListValue& pair = entry.GetList();
      CHECK_EQ(pair.size(), 2u);
      sizes.emplace_back(pair[0].GetInt(), pair[1].GetInt());
    }
    return sizes;
  }

  gfx::Size GetViewportSize(content::WebContents* web_contents) {
    const content::EvalJsResult result = content::EvalJs(
        web_contents, "[window.innerWidth, window.innerHeight]");
    const base::ListValue& pair = result.ExtractList();
    CHECK_EQ(pair.size(), 2u);
    return gfx::Size(pair[0].GetInt(), pair[1].GetInt());
  }

  bool WaitForAnimationFrame(content::WebContents* web_contents) {
    return content::ExecJs(web_contents,
                           "new Promise(resolve => "
                           "requestAnimationFrame(() => "
                           "requestAnimationFrame(resolve)))");
  }

  void CreateSplitTabAndLoadReLayoutTestPage() {
    CreateSplitView();
    LoadReLayoutTestPageInActiveSplitTabs();
  }

  void CreateSplitView() {
    auto* tab_strip_model = browser()->GetTabStripModel();
    const int active_index = tab_strip_model->active_index();

    RunScheduledLayouts();
    chrome::NewSplitTab(browser(), split_tabs::SplitTabLayout::kSideBySide,
                        split_tabs::SplitTabCreatedSource::kToolbarButton);
    EXPECT_TRUE(content::WaitForLoadStop(
        tab_strip_model->GetWebContentsAt(active_index + 1)));
    RunScheduledLayouts();
  }

  void LoadReLayoutTestPageInActiveSplitTabs() {
    auto* tab_strip_model = browser()->GetTabStripModel();
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

IN_PROC_BROWSER_TEST_F(
    MultiContentsViewWebContentsReLayoutBrowserTest,
    SwitchingTabsShouldNotTriggerWebContentsReLayout_SplitNoSplit) {
  auto* tab_strip_model = browser()->GetTabStripModel();

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
  auto* tab_strip_model = browser()->GetTabStripModel();

  const GURL test_url = embedded_test_server()->GetURL(kReLayoutTestURL);

  // Open split view and test page.
  CreateSplitTabAndLoadReLayoutTestPage();

  // Focus on the split tab.
  tab_strip_model->GetWebContentsAt(1)->Focus();

  // Add a dummy non-split tab to prevent NTP redirection.
  EXPECT_TRUE(
      AddTabAtIndex(2, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  tab_strip_model->GetWebContentsAt(1)->Focus();

  // Add a new tab and open split view.
  EXPECT_TRUE(
      AddTabAtIndex(3, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
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
      3, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  RunScheduledLayouts();

  // No resize should have happened in the web contents.
  CheckNoResizeHappened();
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewWebContentsReLayoutBrowserTest,
                       EnterAndExitFullscreenInSplitTabShouldResizeDirectly) {
#if BUILDFLAG(IS_MAC)
  // Native macOS fullscreen animations emit intermediate NSWindow bounds that
  // vary asynchronously across runs; fake fullscreen keeps transitions
  // deterministic in tests.
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif

  auto* tab_strip_model = browser()->GetTabStripModel();

  CreateSplitView();

  // Change the size.
  multi_contents_view()->OnResize(multi_contents_view()->width() * 0.3, true);
  RunScheduledLayouts();

  // Load the test page in the active tab and split tab.
  LoadReLayoutTestPageInActiveSplitTabs();
  RunScheduledLayouts();

  // Focus on the split tab.
  content::WebContents* split_tab = tab_strip_model->GetWebContentsAt(1);
  split_tab->Focus();
  RunScheduledLayouts();

  // Wait for an initial compositor frame so any pending navigation visual
  // properties ACKs settle before entering fullscreen.
  const gfx::Size initial_size = split_tab->GetSize();
  ASSERT_TRUE(base::test::RunUntil([this, split_tab, initial_size]() {
    return GetViewportSize(split_tab) == initial_size;
  }));
  ASSERT_TRUE(WaitForAnimationFrame(split_tab));
  ASSERT_TRUE(GetResizeSizes(split_tab).empty());

  const int pre_fullscreen_contents_width = multi_contents_view()->width();
  ASSERT_LT(initial_size.width(), pre_fullscreen_contents_width / 2);

  // Enter fullscreen in the split tab.
  {
    auto waiter = ui_test_utils::CreateAsyncWidgetRequestWaiter(*browser());
    split_tab->GetDelegate()->EnterFullscreenModeForTab(
        split_tab->GetPrimaryMainFrame(), {});
    ui_test_utils::FullscreenWaiter(browser(), {.tab_fullscreen = true}).Wait();
    waiter.Wait();
  }
  RunScheduledLayouts();

  // Wait until Blink commits the settled fullscreen viewport size.
  ASSERT_TRUE(base::test::RunUntil(
      [this, split_tab, pre_fullscreen_contents_width, initial_size]() {
        const gfx::Size target_size = split_tab->GetSize();
        return !GetResizeSizes(split_tab).empty() &&
               target_size.width() >= pre_fullscreen_contents_width - 2 &&
               target_size.height() > initial_size.height() &&
               GetViewportSize(split_tab) == target_size;
      }));
  ASSERT_TRUE(WaitForAnimationFrame(split_tab));

  const std::vector<gfx::Size> enter_resizes = GetResizeSizes(split_tab);
  ASSERT_FALSE(enter_resizes.empty());
  for (const gfx::Size& size : enter_resizes) {
    // Entering fullscreen from split view may emit multiple compositor-driven
    // resizes on Wayland (immediate layout + async configure), but must never
    // size split_tab to single-tab width while the toolbar is still visible or
    // at split width with fullscreen height.
    EXPECT_GE(size.width(), pre_fullscreen_contents_width - 2);
    EXPECT_GT(size.height(), initial_size.height());
  }

  // Exit fullscreen in the split tab.
  {
    auto waiter = ui_test_utils::CreateAsyncWidgetRequestWaiter(*browser());
    split_tab->GetDelegate()->ExitFullscreenModeForTab(split_tab);
    ui_test_utils::FullscreenWaiter(
        browser(), ui_test_utils::FullscreenWaiter::kNoFullscreen)
        .Wait();
    waiter.Wait();
  }
  RunScheduledLayouts();

  ASSERT_TRUE(
      base::test::RunUntil([this, split_tab, &enter_resizes, initial_size]() {
        return GetResizeSizes(split_tab).size() > enter_resizes.size() &&
               GetViewportSize(split_tab) == initial_size;
      }));
  ASSERT_TRUE(WaitForAnimationFrame(split_tab));

  const std::vector<gfx::Size> all_resizes = GetResizeSizes(split_tab);
  ASSERT_GT(all_resizes.size(), enter_resizes.size());
  for (size_t i = enter_resizes.size(); i < all_resizes.size(); ++i) {
    // Exiting fullscreen must restore the split ratio directly without an
    // intermediate single-tab-width re-layout.
    EXPECT_LT(all_resizes[i].width(), pre_fullscreen_contents_width / 2);
  }
  EXPECT_EQ(all_resizes.back(), initial_size);
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest, OnlyFocusTabsInSplitView) {
  // Set up tab strip with a regular tab and two split views with the last split
  // view being active.
  auto* tab_strip_model = browser()->GetTabStripModel();

  EXPECT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  chrome::NewSplitTab(browser(), split_tabs::SplitTabLayout::kSideBySide,
                      split_tabs::SplitTabCreatedSource::kToolbarButton);
  EXPECT_TRUE(
      AddTabAtIndex(3, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  chrome::NewSplitTab(browser(), split_tabs::SplitTabLayout::kSideBySide,
                      split_tabs::SplitTabCreatedSource::kToolbarButton);

  ASSERT_EQ(5, browser()->GetTabStripModel()->count());
  const int active_index = tab_strip_model->active_index();
  ASSERT_EQ(4, active_index);
  EXPECT_TRUE(tab_strip_model->GetActiveTab()->IsSplit());
  EXPECT_FALSE(tab_strip_model->GetTabAtIndex(0)->IsSplit());
  EXPECT_TRUE(tab_strip_model->GetTabAtIndex(1)->IsSplit());

  auto* delegate = multi_contents_view()->delegate_for_testing();
  // Focusing a tab outside the active split doesn't change the active index.
  delegate->WebContentsFocused(tab_strip_model->GetWebContentsAt(0));
  EXPECT_EQ(tab_strip_model->active_index(), active_index);

  // Focusing a split tab outside the active split doesn't change the active
  // index.
  delegate->WebContentsFocused(tab_strip_model->GetWebContentsAt(1));
  EXPECT_EQ(tab_strip_model->active_index(), active_index);

  // Focusing a tab inside the active split changes the active index.
  delegate->WebContentsFocused(tab_strip_model->GetWebContentsAt(3));
  EXPECT_EQ(tab_strip_model->active_index(), 3);
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest, LeadingSeparatorLayout) {
  MultiContentsView* view = multi_contents_view();
  view->SetShouldShowTopSeparator(true);
  view->drop_target_view_->Show(
      MultiContentsDropTargetView::DropSide::START,
      MultiContentsDropTargetView::DropTargetState::kFull,
      MultiContentsDropTargetView::DragType::kLink);
  view->drop_target_view_->animation_for_testing().End();

  gfx::Rect initial_bounds(10, 20, 100, 80);
  views::ProposedLayout::ChildLayoutList actual_child_layouts;

  gfx::Rect remaining_space =
      view->CalculateSeparatorLayouts(initial_bounds, actual_child_layouts);

  constexpr int kSeparatorThickness = views::Separator::kThickness;

  gfx::Rect expected_remaining_space(
      initial_bounds.x() + kSeparatorThickness,
      initial_bounds.y() + kSeparatorThickness,
      initial_bounds.width() - kSeparatorThickness,
      initial_bounds.height() - kSeparatorThickness);
  EXPECT_EQ(expected_remaining_space, remaining_space);

  views::ProposedLayout::ChildLayoutList expected_separator_layouts;
  expected_separator_layouts.emplace_back(
      view->contents_separators_.top_separator.get(), true,
      gfx::Rect(10, 20, 100, kSeparatorThickness));
  expected_separator_layouts.emplace_back(
      view->contents_separators_.leading_separator.get(), true,
      gfx::Rect(10, 20, kSeparatorThickness, 80));
  expected_separator_layouts.emplace_back(
      view->contents_separators_.trailing_separator.get(), false,
      gfx::Rect(10 + 100, 20, 0, 80));
  expected_separator_layouts.emplace_back(
      view->contents_separators_.corner_separator.get(), true,
      gfx::Rect(
          initial_bounds.origin(),
          view->contents_separators_.corner_separator->GetPreferredSize()));

  CompareLayouts(expected_separator_layouts, actual_child_layouts);
  EXPECT_EQ(
      CornerOrientation::kTopLeading,
      view->contents_separators_.corner_separator->orientation_for_testing());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest, TrailingSeparatorLayout) {
  MultiContentsView* view = multi_contents_view();
  view->SetShouldShowTopSeparator(true);
  view->drop_target_view_->Show(
      MultiContentsDropTargetView::DropSide::END,
      MultiContentsDropTargetView::DropTargetState::kFull,
      MultiContentsDropTargetView::DragType::kLink);
  view->drop_target_view_->animation_for_testing().End();

  gfx::Rect initial_bounds(10, 20, 100, 80);
  views::ProposedLayout::ChildLayoutList actual_child_layouts;

  gfx::Rect remaining_space =
      view->CalculateSeparatorLayouts(initial_bounds, actual_child_layouts);

  constexpr int kSeparatorThickness = views::Separator::kThickness;

  gfx::Rect expected_remaining_space(
      initial_bounds.x(), initial_bounds.y() + kSeparatorThickness,
      initial_bounds.width() - kSeparatorThickness,
      initial_bounds.height() - kSeparatorThickness);
  EXPECT_EQ(expected_remaining_space, remaining_space);

  views::ProposedLayout::ChildLayoutList expected_separator_layouts;
  expected_separator_layouts.emplace_back(
      view->contents_separators_.top_separator.get(), true,
      gfx::Rect(10, 20, 100, kSeparatorThickness));
  expected_separator_layouts.emplace_back(
      view->contents_separators_.leading_separator.get(), false,
      gfx::Rect(10, 20, 0, 80));
  expected_separator_layouts.emplace_back(
      view->contents_separators_.trailing_separator.get(), true,
      gfx::Rect(10 + 100 - kSeparatorThickness, 20, kSeparatorThickness, 80));
  expected_separator_layouts.emplace_back(
      view->contents_separators_.corner_separator.get(), true,
      gfx::Rect(
          gfx::Point(initial_bounds.right() -
                         view->contents_separators_.corner_separator
                             ->GetPreferredSize()
                             .width(),
                     initial_bounds.y()),
          view->contents_separators_.corner_separator->GetPreferredSize()));

  CompareLayouts(expected_separator_layouts, actual_child_layouts);
  EXPECT_EQ(
      CornerOrientation::kTopTrailing,
      view->contents_separators_.corner_separator->orientation_for_testing());
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest, DropTargetLayout) {
  MultiContentsView* view = multi_contents_view();
  gfx::Rect initial_bounds(10, 20, 1000, 800);

  // Drop target hidden.
  {
    views::ProposedLayout::ChildLayoutList actual_child_layouts;
    view->drop_target_view_->SetVisible(false);
    view->drop_target_view_->animation_for_testing().End();
    gfx::Rect remaining_space =
        view->CalculateDropTargetLayout(initial_bounds, actual_child_layouts);

    EXPECT_EQ(initial_bounds, remaining_space);
    EXPECT_EQ(1u, actual_child_layouts.size());
    EXPECT_EQ(view->drop_target_view_.get(),
              actual_child_layouts[0].child_view);
    EXPECT_FALSE(actual_child_layouts[0].visible);
  }

  // Drop target is on the START side.
  {
    views::ProposedLayout::ChildLayoutList actual_child_layouts;
    view->drop_target_view_->Show(
        MultiContentsDropTargetView::DropSide::START,
        MultiContentsDropTargetView::DropTargetState::kFull,
        MultiContentsDropTargetView::DragType::kLink);
    view->drop_target_view_->animation_for_testing().End();
    gfx::Rect remaining_space =
        view->CalculateDropTargetLayout(initial_bounds, actual_child_layouts);

    const int drop_target_width =
        view->drop_target_view_->GetSizeForAvailableSpace(
            initial_bounds.width());
    gfx::Rect expected_remaining_space(
        initial_bounds.x() + drop_target_width, initial_bounds.y(),
        initial_bounds.width() - drop_target_width, initial_bounds.height());
    EXPECT_EQ(expected_remaining_space, remaining_space);

    views::ProposedLayout::ChildLayoutList expected_child_layouts;
    expected_child_layouts.emplace_back(
        view->drop_target_view_.get(), true,
        gfx::Rect(initial_bounds.x(), initial_bounds.y(), drop_target_width,
                  initial_bounds.height()));
    CompareLayouts(expected_child_layouts, actual_child_layouts);
  }

  // Drop target is on the END side.
  {
    views::ProposedLayout::ChildLayoutList actual_child_layouts;
    view->drop_target_view_->Show(
        MultiContentsDropTargetView::DropSide::END,
        MultiContentsDropTargetView::DropTargetState::kFull,
        MultiContentsDropTargetView::DragType::kLink);
    view->drop_target_view_->animation_for_testing().End();
    gfx::Rect remaining_space =
        view->CalculateDropTargetLayout(initial_bounds, actual_child_layouts);

    const int drop_target_width =
        view->drop_target_view_->GetSizeForAvailableSpace(
            initial_bounds.width());
    gfx::Rect expected_remaining_space(
        initial_bounds.x(), initial_bounds.y(),
        initial_bounds.width() - drop_target_width, initial_bounds.height());
    EXPECT_EQ(expected_remaining_space, remaining_space);

    views::ProposedLayout::ChildLayoutList expected_child_layouts;
    expected_child_layouts.emplace_back(
        view->drop_target_view_.get(), true,
        gfx::Rect(initial_bounds.right() - drop_target_width,
                  initial_bounds.y(), drop_target_width,
                  initial_bounds.height()));
    CompareLayouts(expected_child_layouts, actual_child_layouts);
  }

  // Drop target is on the BOTTOM side.
  {
    views::ProposedLayout::ChildLayoutList actual_child_layouts;
    view->drop_target_view_->Show(
        MultiContentsDropTargetView::DropSide::BOTTOM,
        MultiContentsDropTargetView::DropTargetState::kFull,
        MultiContentsDropTargetView::DragType::kLink);
    view->drop_target_view_->animation_for_testing().End();
    gfx::Rect remaining_space =
        view->CalculateDropTargetLayout(initial_bounds, actual_child_layouts);

    const int drop_target_height =
        view->drop_target_view_->GetSizeForAvailableSpace(
            initial_bounds.height());
    gfx::Rect expected_remaining_space(
        initial_bounds.x(), initial_bounds.y(), initial_bounds.width(),
        initial_bounds.height() - drop_target_height);
    EXPECT_EQ(expected_remaining_space, remaining_space);

    views::ProposedLayout::ChildLayoutList expected_child_layouts;
    expected_child_layouts.emplace_back(
        view->drop_target_view_.get(), true,
        gfx::Rect(initial_bounds.x(),
                  initial_bounds.bottom() - drop_target_height,
                  initial_bounds.width(), drop_target_height));
    CompareLayouts(expected_child_layouts, actual_child_layouts);
  }
}

IN_PROC_BROWSER_TEST_F(MultiContentsViewBrowserTest, TopSeparatorLayerUsage) {
  MultiContentsView* view = multi_contents_view();
  view->SetShouldShowTopSeparator(true);
  RunScheduledLayouts();

  // In normal single-tab mode, background_view_ is hidden so top_separator
  // should not paint to its own layer.
  EXPECT_FALSE(view->contents_separators_.top_separator->layer());

  // Showing the drop target makes background_view_ visible, so top_separator
  // should switch to a layer to stay above background_view_.
  view->drop_target_view_->Show(
      MultiContentsDropTargetView::DropSide::START,
      MultiContentsDropTargetView::DropTargetState::kFull,
      MultiContentsDropTargetView::DragType::kLink);
  view->drop_target_view_->animation_for_testing().End();
  RunScheduledLayouts();
  EXPECT_TRUE(view->contents_separators_.top_separator->layer());

  // Hiding the drop target hides background_view_, so top_separator should
  // destroy its layer.
  view->drop_target_view_->Hide(/*suppress_animation=*/true);
  RunScheduledLayouts();
  EXPECT_FALSE(view->contents_separators_.top_separator->layer());

  // In split view, top_separator is hidden so it should not have a layer.
  chrome::NewSplitTab(browser(), split_tabs::SplitTabLayout::kSideBySide,
                      split_tabs::SplitTabCreatedSource::kToolbarButton);
  RunScheduledLayouts();
  EXPECT_FALSE(view->contents_separators_.top_separator->layer());
}
