// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/immersive/immersive_mode_controller.h"

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cstdlib>
#include <tuple>
#include <utility>

#include "base/apple/foundation_util.h"
#import "base/mac/mac_util.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_mac.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"

// A window that only keeps the immersive toolbar revealed while it is key, like
// the omnibox popup.
@interface KeyOnlyRevealLockTestWindow : NativeWidgetMacNSWindow
@end

@implementation KeyOnlyRevealLockTestWindow
- (BOOL)immersiveRevealLockRequiresKey {
  return YES;
}
@end

namespace {

class KeyOnlyRevealLockNativeWidgetMac : public views::NativeWidgetMac {
 public:
  using views::NativeWidgetMac::NativeWidgetMac;

 protected:
  NativeWidgetMacNSWindow* CreateNSWindow(
      const remote_cocoa::mojom::CreateWindowParams* params) override {
    KeyOnlyRevealLockTestWindow* window = [[KeyOnlyRevealLockTestWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:params->style_mask
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window.releasedWhenClosed = NO;
    return window;
  }
};

}  // namespace

class ScopedAlwaysShowToolbar {
 public:
  ScopedAlwaysShowToolbar(BrowserWindowInterface* browser, bool always_show) {
    prefs_ = browser->GetProfile()->GetPrefs();
    original_ = prefs_->GetBoolean(prefs::kShowFullscreenToolbar);
    prefs_->SetBoolean(prefs::kShowFullscreenToolbar, always_show);
  }
  ~ScopedAlwaysShowToolbar() {
    prefs_->SetBoolean(prefs::kShowFullscreenToolbar, original_);
  }

 private:
  raw_ptr<PrefService> prefs_;
  bool original_;
};

class ImmersiveModeControllerMacInteractiveTest : public InProcessBrowserTest {
 public:
  ImmersiveModeControllerMacInteractiveTest() = default;

  ImmersiveModeControllerMacInteractiveTest(
      const ImmersiveModeControllerMacInteractiveTest&) = delete;
  ImmersiveModeControllerMacInteractiveTest& operator=(
      const ImmersiveModeControllerMacInteractiveTest&) = delete;

  NSView* GetMovedContentViewForWidget(views::Widget* overlay_widget) {
    return (__bridge NSView*)overlay_widget->GetNativeWindowProperty(
        views::NativeWidgetMacNSWindowHost::kMovedContentNSView);
  }

  // Convenience function to get the NSWindow from the browser window.
  NSWindow* browser_window() {
    return browser()->GetWindow()->GetNativeWindow().GetNativeNSWindow();
  }

  // Creates a new widget as a child of the first browser window and brings it
  // onscreen.
  void CreateAndShowWidgetOnFirstBrowserWindow() {
    NSUInteger starting_child_window_count =
        browser_window().childWindows.count;

    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(100, 100, 200, 200);
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    params.parent = browser_view->GetWidget()->GetNativeView();
    params.z_order = ui::ZOrderLevel::kNormal;

    params.delegate = new views::WidgetDelegateView(
        views::WidgetDelegateView::CreatePassKey());

    widget_ = std::make_unique<views::Widget>();
    widget_->Init(std::move(params));

    views::View* root_view = widget_->GetRootView();
    root_view->SetBackground(views::CreateSolidBackground(SK_ColorRED));

    widget_->Show();

    // The browser should have one more child window.
    EXPECT_EQ(starting_child_window_count + 1,
              browser_window().childWindows.count);
  }

  void HideWidget() { widget_->Hide(); }

  void CreateSecondBrowserWindow() {
    this->second_browser_ = CreateBrowser(browser()->GetProfile());
  }

  // Makes the second browser window the active window and ensures it's on the
  // active space.
  void ActivateSecondBrowserWindow() {
    views::test::PropertyWaiter activate_waiter(
        base::BindRepeating(&ui::BaseWindow::IsActive,
                            base::Unretained(second_browser_->GetWindow())),
        true);
    second_browser_->GetWindow()->Activate();
    EXPECT_TRUE(activate_waiter.Wait());

    views::test::PropertyWaiter active_space_waiter(
        base::BindRepeating(&ImmersiveModeControllerMacInteractiveTest::
                                SecondBrowserWindowIsOnTheActiveSpace,
                            base::Unretained(this)),
        true);
    EXPECT_TRUE(active_space_waiter.Wait());
  }

  bool SecondBrowserWindowIsOnTheActiveSpace() {
    return second_browser_->GetWindow()
        ->GetNativeWindow()
        .GetNativeNSWindow()
        .onActiveSpace;
  }

  bool WidgetIsVisible() {
    return widget_->GetNativeWindow().GetNativeNSWindow().visible;
  }

  void CleanUp() {
    // Let the test harness free the second browser.
    second_browser_ = nullptr;
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<BrowserWindowInterface> second_browser_ = nullptr;
};

// Tests that the browser can be toggled into and out of immersive fullscreen,
// and that proper connections are maintained.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       ToggleFullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* overlay_widget = browser_view->overlay_widget();

  NSView* overlay_widget_content_view =
      overlay_widget->GetNativeWindow().GetNativeNSWindow().contentView;
  NSWindow* overlay_widget_window = [overlay_widget_content_view window];

  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget), nullptr);
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  FullscreenController* fullscreen_controller =
      ExclusiveAccessManager::From(browser())->fullscreen_controller();

  EXPECT_TRUE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget),
            overlay_widget_content_view);

  EXPECT_NE(overlay_widget_window.contentView, overlay_widget_content_view);

  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget), nullptr);
  EXPECT_EQ(overlay_widget_window.contentView, overlay_widget_content_view);
}

// Tests that the tab strip is placed at the origin of the tab overlay view when
// immersive fullscreen is enabled, even if no fullscreen layout pass ran before
// it was reparented. Space for the traffic lights is reserved by the tab strip
// border in the overlay, so any offset carried over from the non-fullscreen
// layout would leave an extra gap. See https://crbug.com/574977354.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       TabStripAtOriginOfTabOverlay) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  if (!browser_view->tab_overlay_view()) {
    GTEST_SKIP() << "Tab strip is not hosted in a separate overlay.";
  }
  views::View* const tab_strip = browser_view->tab_strip_view();

  // In the non-fullscreen layout, the tab strip is offset to make room for the
  // traffic lights.
  RunScheduledLayouts();
  ASSERT_EQ(tab_strip->parent(), browser_view);
  ASSERT_GT(tab_strip->x(), 0);

  // Enable immersive mode directly, without first laying out the browser view
  // in a fullscreen state. This simulates the browser view layout being
  // deferred during the fullscreen transition, as happens when a new window
  // enters fullscreen before it becomes visible.
  auto* const controller = ImmersiveModeController::From(browser());
  controller->SetEnabled(true);
  ASSERT_EQ(tab_strip->parent(), browser_view->tab_overlay_view());
  EXPECT_EQ(tab_strip->origin(), gfx::Point());

  // The origin should stay put after any subsequent layout.
  RunScheduledLayouts();
  EXPECT_EQ(tab_strip->origin(), gfx::Point());

  controller->SetEnabled(false);
}

// Tests that minimum content offset is nonzero iff the find bar is shown and
// "Always Show Toolbar in Full Screen" is off.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       MinimumContentOffset) {
  base::AutoReset<bool> enable_animation_for_test =
      FindBarHost::SetEnableAnimationsForTesting(false);

  auto* const controller = ImmersiveModeController::From(browser());
  controller->SetEnabled(true);
  {
    ScopedAlwaysShowToolbar scoped_always_show(browser(), false);
    EXPECT_EQ(controller->GetMinimumContentOffset(), 0);

    {
      views::NamedWidgetShownWaiter shown_waiter(
          views::test::AnyWidgetTestPasskey{}, "FindBarHost");
      chrome::Find(browser());
      std::ignore = shown_waiter.WaitIfNeededAndGet();
      EXPECT_GT(controller->GetMinimumContentOffset(), 0);
    }

    chrome::CloseFind(browser());
    EXPECT_EQ(controller->GetMinimumContentOffset(), 0);
  }
  {
    // Now, with "Always Show..." on
    ScopedAlwaysShowToolbar scoped_always_show(browser(), true);
    {
      views::NamedWidgetShownWaiter shown_waiter(
          views::test::AnyWidgetTestPasskey{}, "FindBarHost");
      chrome::Find(browser());
      std::ignore = shown_waiter.WaitIfNeededAndGet();
      EXPECT_EQ(controller->GetMinimumContentOffset(), 0);
    }
    chrome::CloseFind(browser());
    EXPECT_EQ(controller->GetMinimumContentOffset(), 0);
  }
}

IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       ExtraInfobarOffset) {
  ScopedAlwaysShowToolbar scoped_always_show(browser(), false);

  // Note that setting the immersive mode controller to "on" without making the
  // window fullscreen may cause some minor discrepancies in layout. They should
  // not adversely affect this test.
  ImmersiveModeControllerMac* controller =
      reinterpret_cast<ImmersiveModeControllerMac*>(
          ImmersiveModeController::From(browser()));
  controller->SetEnabled(true);

  controller->OnImmersiveModeMenuBarRevealChanged(0);
  RunScheduledLayouts();
  controller->OnAutohidingMenuBarHeightChanged(0);
  EXPECT_EQ(controller->GetExtraInfobarOffset(), 0);

  controller->OnImmersiveModeMenuBarRevealChanged(0.5);
  RunScheduledLayouts();
  int half_revealed = controller->GetExtraInfobarOffset();
  EXPECT_GT(half_revealed, 0);

  controller->OnImmersiveModeMenuBarRevealChanged(1);
  RunScheduledLayouts();
  int revealed = controller->GetExtraInfobarOffset();
  // The size may be even or odd, in which case one of these is true.
  EXPECT_GE(revealed, half_revealed * 2);
  EXPECT_LE(revealed, half_revealed * 2 + 1);

  // Now with non-zero menubar.
  controller->OnAutohidingMenuBarHeightChanged(30);
  EXPECT_EQ(controller->GetExtraInfobarOffset(), revealed + 30);

  controller->OnImmersiveModeMenuBarRevealChanged(0.5);
  RunScheduledLayouts();
  EXPECT_EQ(controller->GetExtraInfobarOffset(), half_revealed + 15);

  controller->OnImmersiveModeMenuBarRevealChanged(0);
  RunScheduledLayouts();
  EXPECT_EQ(controller->GetExtraInfobarOffset(), 0);
}

// Tests that ordering a child window out on a fullscreen window when that
// window is not on the active space does not trigger a space switch.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       NoSpaceSwitch) {
  // Create a new browser window for later.
  CreateSecondBrowserWindow();

  // Move the original browser window into its own fullscreen space.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  // Add a widget to the original browser window.
  CreateAndShowWidgetOnFirstBrowserWindow();

  // Switch to the second browser window, which will take us out of the
  // fullscreen space.
  ActivateSecondBrowserWindow();

  // Hide the widget. This would typically cause a space switch to the
  // fullscreen space. http://crbug.com/40272387 stops the space switch from
  // happening.
  HideWidget();

  // The space switch happens out of process and asynchronously. We want to make
  // sure the space switch doesn't happen, which means waiting for a bit. In the
  // expected case we will trip PropertyWaiter timeout. If this ends up being
  // flakey we need to extend the timeout or find a different approach for
  // testing.
  views::test::PropertyWaiter activate_waiter(
      base::BindRepeating(&ui::BaseWindow::IsActive,
                          base::Unretained(browser()->GetWindow())),
      true);
  EXPECT_FALSE(activate_waiter.Wait());

  // We should still be on the original space (no sudden space change).
  EXPECT_TRUE(SecondBrowserWindowIsOnTheActiveSpace());

  CleanUp();
}

// Tests that the browser can be toggled into and out of immersive fullscreen
// with vertical tabs enabled, and that proper connections are maintained.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       ToggleFullscreenWithVerticalTabstrip) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  views::Widget* overlay_widget = browser_view->overlay_widget();

  NSView* overlay_widget_content_view =
      overlay_widget->GetNativeWindow().GetNativeNSWindow().contentView;
  NSWindow* overlay_widget_window = [overlay_widget_content_view window];

  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget), nullptr);
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  FullscreenController* fullscreen_controller =
      ExclusiveAccessManager::From(browser())->fullscreen_controller();

  EXPECT_TRUE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget),
            overlay_widget_content_view);

  EXPECT_NE(overlay_widget_window.contentView, overlay_widget_content_view);

  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget), nullptr);
  EXPECT_EQ(overlay_widget_window.contentView, overlay_widget_content_view);
}

// Tests that the browser does not crash when toggling between vertical and
// horizontal tab layouts.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       ToggleHorizontalVerticalTabLayout) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  views::Widget* overlay_widget = browser_view->overlay_widget();

  NSView* overlay_widget_content_view =
      overlay_widget->GetNativeWindow().GetNativeNSWindow().contentView;
  NSWindow* overlay_widget_window = [overlay_widget_content_view window];

  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget), nullptr);
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  FullscreenController* fullscreen_controller =
      ExclusiveAccessManager::From(browser())->fullscreen_controller();

  EXPECT_TRUE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget),
            overlay_widget_content_view);

  EXPECT_NE(overlay_widget_window.contentView, overlay_widget_content_view);

  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(false);
  RunScheduledLayouts();

  EXPECT_TRUE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget),
            overlay_widget_content_view);
}

// NSWindow category for the private `-_rebuildOrderingGroup:` method.
@interface NSWindow (RebuildOrderingGroupTest)
- (void)_rebuildOrderingGroup:(BOOL)isVisible;
@end

// Helper class for the RebuildOrderingGroup test.
@interface RebuildOrderingGroupTestWindow : NSWindow {
 @public
  BOOL _orderingGroupRebuilt;
}
@end

@implementation RebuildOrderingGroupTestWindow

- (void)_rebuildOrderingGroup:(BOOL)isVisible {
  _orderingGroupRebuilt = YES;
  [super _rebuildOrderingGroup:isVisible];
}

@end

// Tests that an -orderOut: or a -close result in an ordering group rebuild of
// the parent. The rebuild behavior is relied upon by a workaround to
// https://crbug.com/40272387. If this test starts failing, the workaround for
// that issue will need to be revisited.
//
// TODO(https://crbug.com/40272387): Remove this test when Apple fixes
// FB13529873.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       RebuildOrderingGroup) {
  // This is the window under test. We want to make sure
  // `-_rebuildOrderingGroup:` is called  during a child's `-orderOut:` or
  // `-close` or during the parent's `-removeChildWindow:`.
  RebuildOrderingGroupTestWindow* testWindow =
      [[RebuildOrderingGroupTestWindow alloc]
          initWithContentRect:NSMakeRect(0, 0, 300, 200)
                    styleMask:NSWindowStyleMaskBorderless
                      backing:NSBackingStoreBuffered
                        defer:NO];
  testWindow.releasedWhenClosed = NO;
  testWindow.backgroundColor = NSColor.redColor;
  [testWindow orderFront:nil];
  EXPECT_TRUE(testWindow.visible);

  // Create a popup window and make it a child of the test window.
  NSWindow* popupWindow =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 50, 50)
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  popupWindow.releasedWhenClosed = NO;
  popupWindow.backgroundColor = NSColor.greenColor;
  [testWindow addChildWindow:popupWindow ordered:NSWindowAbove];
  EXPECT_TRUE(popupWindow.visible);

  // Reset the ordering group rebuilt flag and make sure it get set during
  // `-orderOut:`.
  testWindow->_orderingGroupRebuilt = NO;
  [popupWindow orderOut:nil];
  EXPECT_TRUE(testWindow->_orderingGroupRebuilt);

  // Re-add the popup window as child of the test window.
  [testWindow addChildWindow:popupWindow ordered:NSWindowAbove];
  EXPECT_TRUE(popupWindow.visible);

  // Reset the ordering group rebuilt flag and make sure it get set during
  // `-close`.
  testWindow->_orderingGroupRebuilt = NO;
  [popupWindow close];
  EXPECT_TRUE(testWindow->_orderingGroupRebuilt);

  // Re-add the popup window as child of the test window, then ensure that the
  // ordering group is rebuilt when the test window removes the popup window.
  [testWindow addChildWindow:popupWindow ordered:NSWindowAbove];
  EXPECT_TRUE(popupWindow.visible);
  testWindow->_orderingGroupRebuilt = NO;
  [testWindow removeChildWindow:popupWindow];
  EXPECT_TRUE(testWindow->_orderingGroupRebuilt);

  // Cleanup
  [popupWindow close];
  [testWindow close];
}

IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       ContentFullscreenChildren) {
  base::AutoReset<bool> enable_animation_for_test =
      FindBarHost::SetEnableAnimationsForTesting(false);

  // Enter browser fullscreen.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  // Open the find bar
  views::NamedWidgetShownWaiter shown_waiter(
      views::test::AnyWidgetTestPasskey{}, "FindBarHost");
  chrome::Find(browser());
  views::Widget* find_bar = shown_waiter.WaitIfNeededAndGet();

  // The find bar should be a child of the overlay widget.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  EXPECT_EQ(browser_view->overlay_widget(), find_bar->parent());

  // Enter content fullscreen. The find bar should move to become a child of the
  // browser widget.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab->GetDelegate()->EnterFullscreenModeForTab(tab->GetPrimaryMainFrame(), {});
  EXPECT_EQ(browser_view->GetWidget(), find_bar->parent());

  // Leave content fullscreen (back to browser fullscreen), the find bar should
  // move back to the overlay widget.
  tab->GetDelegate()->ExitFullscreenModeForTab(tab);
  EXPECT_EQ(browser_view->overlay_widget(), find_bar->parent());

  chrome::CloseFind(browser());
}

// Regression test for crbug.com/431671448. Asserts that the Browser is able to
// tollerate the system destroying the overlay widget before the Browser widget.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       HandlesOverlayWidgetDestruction) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* overlay_widget = browser_view->overlay_widget();

  // Transition to fullscreen.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  FullscreenController* fullscreen_controller =
      ExclusiveAccessManager::From(browser())->fullscreen_controller();
  EXPECT_TRUE(fullscreen_controller->IsFullscreenForBrowser());

  // Simulate a synchronous destruction of the overlay widget. This should not
  // crash.
  overlay_widget->CloseNow();

  // Transition out of fullscreen.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
}

// Tests that bubbles anchored to the app menu button are correctly re-anchored
// when entering immersive fullscreen, and that the content offset is updated.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       BubbleAnchoring) {
  // Disable "Always Show Toolbar in Full Screen"
  ScopedAlwaysShowToolbar scoped_always_show(browser(), false);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* anchor_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kToolbarAppMenuButtonElementId, browser_view->GetElementContext());

  // Create and show a bubble anchored to the app menu button.
  auto delegate = std::make_unique<views::BubbleDialogDelegate>(
      anchor_view, views::BubbleBorder::TOP_RIGHT);
  delegate->SetContentsView(std::make_unique<views::View>())
      ->SetPreferredSize(gfx::Size(100, 100));
  views::Widget* bubble_widget =
      views::BubbleDialogDelegate::CreateBubbleDeprecated(
          std::move(delegate),
          views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  bubble_widget->Show();

  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  ImmersiveModeControllerMac* controller =
      static_cast<ImmersiveModeControllerMac*>(
          ImmersiveModeController::From(browser()));

  // Verify that the bubble caused the toolbar to be revealed.
  EXPECT_TRUE(controller->IsRevealed());

  // Verify that the content is offset to make room for the toolbar.
  EXPECT_GT(controller->GetMinimumContentOffset(), 0);

  // Verify the bubble is anchored correctly.
  gfx::Rect anchor_bounds = anchor_view->GetBoundsInScreen();
  gfx::Rect bubble_bounds = bubble_widget->GetWindowBoundsInScreen();

  EXPECT_GE(bubble_bounds.y(), anchor_bounds.bottom());
  EXPECT_GT(bubble_bounds.y(), 0);
  EXPECT_GT(bubble_bounds.x(), 0);

  bubble_widget->CloseNow();

  // Verify MinimumContentOffset resets to 0.
  EXPECT_EQ(controller->GetMinimumContentOffset(), 0);
}

IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       VerticalTabsNonClientHitTest) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // Toggle vertical tabs on
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();

  // Enter Fullscreen Immersive mode
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ImmersiveModeController* controller =
      ImmersiveModeController::From(browser());
  EXPECT_TRUE(controller->IsEnabled());

  auto* vertical_tab_strip =
      browser_view->vertical_tab_strip_region_view_for_testing();
  ASSERT_TRUE(vertical_tab_strip);
  EXPECT_TRUE(vertical_tab_strip->GetVisible());

  // Test a point in the empty area of the vertical tab strip
  gfx::Point point_in_view(vertical_tab_strip->width() / 2,
                           vertical_tab_strip->height() - 50);
  gfx::Point screen_point = point_in_view;
  views::View::ConvertPointToScreen(vertical_tab_strip, &screen_point);

  gfx::Point point_in_ncv = screen_point;
  views::View::ConvertPointFromScreen(
      browser_view->GetWidget()->non_client_view(), &point_in_ncv);

  int hit_test = browser_view->GetWidget()->non_client_view()->NonClientHitTest(
      point_in_ncv);

  // In immersive fullscreen with vertical tabs, hitting the empty space of the
  // vertical tab strip should return HTCAPTION.
  EXPECT_EQ(hit_test, HTCAPTION);
}

// Regression test for crbug.com/500609044. Verifies that entering fullscreen
// with horizontal tabs, exiting, switching to vertical tabs, and re-entering
// fullscreen does not leave the tab overlay widget visible. Previously, stale
// tab_native_widget_id_ caused an ImmersiveModeTabbedControllerCocoa to be
// created even with vertical tabs, resulting in a stuck titlebar.
// TODO(crbug.com/542630477): Re-enable this test.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       DISABLED_TabOverlayHiddenAfterSwitchToVerticalTabs) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* tab_overlay_widget = browser_view->tab_overlay_widget();
  ASSERT_TRUE(tab_overlay_widget);

  // Step 1: Start with horizontal tabs (default, VTS pref is false).
  EXPECT_FALSE(browser_view->ShouldDrawVerticalTabStrip());

  // Step 2: Enter fullscreen with horizontal tabs.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  FullscreenController* fullscreen_controller =
      ExclusiveAccessManager::From(browser())->fullscreen_controller();
  EXPECT_TRUE(fullscreen_controller->IsFullscreenForBrowser());

  // The tab overlay widget should be visible with horizontal tabs in
  // fullscreen, and should have non-zero height.
  EXPECT_TRUE(tab_overlay_widget->IsVisible());
  EXPECT_GT(tab_overlay_widget->GetWindowBoundsInScreen().height(), 0);

  // Step 3: Exit fullscreen.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());

  // Step 4: Switch to vertical tabs.
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  RunScheduledLayouts();
  EXPECT_TRUE(browser_view->ShouldDrawVerticalTabStrip());

  // Step 5: Re-enter fullscreen with vertical tabs.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(fullscreen_controller->IsFullscreenForBrowser());

  // The tab overlay widget should NOT be visible with vertical tabs in
  // fullscreen. Before the fix, stale tab_native_widget_id_ caused the
  // ImmersiveModeTabbedControllerCocoa to be created, leaving the tab overlay
  // visible and resulting in a stuck titlebar with a white gap.
  EXPECT_FALSE(tab_overlay_widget->IsVisible());
}

IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       RevealOnTabChangeWithVerticalTabs) {
  ScopedAlwaysShowToolbar scoped_always_show(browser(), false);
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);

  // Add an NTP tab and a regular tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://newtab"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://example.com"),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Enter immersive fullscreen.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ImmersiveModeController* controller =
      ImmersiveModeController::From(browser());
  EXPECT_TRUE(controller->IsEnabled());

  // Switch to the NTP and confirm that the immersive toolbar is releaved
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(controller->IsRevealed());

  // Switch back to the "http://example.com" tab.
  browser()->tab_strip_model()->ActivateTabAt(2);

  // Wait for the reveal to disappear.
  ui_test_utils::CheckWaiter(
      base::BindRepeating(&ImmersiveModeController::IsRevealed,
                          base::Unretained(controller)),
      false, base::Seconds(3))
      .Wait();

  // Confirm that the immersive toolbar isn't revealed.
  EXPECT_FALSE(controller->IsRevealed());
}

// Tests that a child window of the overlay that only keeps the toolbar revealed
// while it is key doesn't hold the toolbar while it isn't key.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       RevealLockRequiresKey) {
  ScopedAlwaysShowToolbar scoped_always_show(browser(), false);
  // Keep focus out of top chrome, which would hold the toolbar revealed when
  // the browser window becomes key again.
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ImmersiveModeController* controller =
      ImmersiveModeController::From(browser());
  auto wait_for_revealed = [controller](bool revealed) {
    ui_test_utils::CheckWaiter(
        base::BindRepeating(&ImmersiveModeController::IsRevealed,
                            base::Unretained(controller)),
        revealed, base::Seconds(3))
        .Wait();
    EXPECT_EQ(controller->IsRevealed(), revealed);
  };
  wait_for_revealed(false);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = browser_view->overlay_widget()->GetNativeView();
  params.bounds = gfx::Rect(100, 100, 200, 200);
  params.native_widget = new KeyOnlyRevealLockNativeWidgetMac(widget.get());
  widget->Init(std::move(params));

  // Shown but not key: the toolbar stays hidden.
  widget->ShowInactive();
  wait_for_revealed(false);

  // Key: the toolbar is revealed.
  views::test::PropertyWaiter widget_active(
      base::BindRepeating(&views::Widget::IsActive,
                          base::Unretained(widget.get())),
      true);
  widget->Activate();
  EXPECT_TRUE(widget_active.Wait());
  wait_for_revealed(true);

  // The browser window takes key back: the toolbar hides again.
  views::test::PropertyWaiter browser_active(
      base::BindRepeating(&views::Widget::IsActive,
                          base::Unretained(browser_view->GetWidget())),
      true);
  browser_view->GetWidget()->Activate();
  EXPECT_TRUE(browser_active.Wait());
  wait_for_revealed(false);

  widget->CloseNow();
}

class ImmersiveModeControllerMacTabStripUnificationInteractiveTest
    : public ImmersiveModeControllerMacInteractiveTest {
 public:
  ImmersiveModeControllerMacTabStripUnificationInteractiveTest() {
    scoped_feature_list_.InitAndEnableFeature(tabs::kTabStripUnification);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Regression test verifying that closing the last tab of a browser window in
// macOS immersive fullscreen (which triggers exiting immersive fullscreen and
// reparenting the tab strip while the closing TabView is still in the view
// tree) does not crash.
IN_PROC_BROWSER_TEST_F(
    ImmersiveModeControllerMacTabStripUnificationInteractiveTest,
    CloseLastTabInFullscreen) {
  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());
  BrowserView* second_browser_view =
      BrowserView::GetBrowserViewForBrowser(second_browser);
  if (!second_browser_view->tab_overlay_view()) {
    GTEST_SKIP() << "Tab strip is not hosted in a separate overlay.";
  }

  ui_test_utils::ToggleFullscreenModeAndWait(second_browser);
  ASSERT_EQ(second_browser_view->tab_strip_view()->parent(),
            second_browser_view->tab_overlay_view());

  ui_test_utils::BrowserDestroyedObserver destroyed_observer(second_browser);
  std::exchange(second_browser, nullptr)->GetTabStripModel()->CloseAllTabs();
  destroyed_observer.Wait();
}

namespace {

// Checks that every time the contents are resized, they extend to the bottom
// of the browser view. A layout that gets this wrong, even if a later layout
// corrects it before the frame is drawn, resizes the web contents to a wrong
// size and the page shows up clipped or with a gap at the bottom.
class ContentsBottomChecker : public views::ViewObserver {
 public:
  explicit ContentsBottomChecker(BrowserView* browser_view)
      : browser_view_(browser_view),
        expected_bottom_inset_(GetContentsBottomInset()) {
    observation_.Observe(browser_view->contents_container());
  }

  int resize_count() const { return resize_count_; }
  int max_error() const { return max_error_; }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    ++resize_count_;
    const int error = GetContentsBottomInset() - expected_bottom_inset_;
    max_error_ = std::max(max_error_, std::abs(error));
  }

 private:
  int GetContentsBottomInset() const {
    views::View* const contents = browser_view_->contents_container();
    return browser_view_->height() -
           views::View::ConvertRectToTarget(contents->parent(), browser_view_,
                                            contents->bounds())
               .bottom();
  }

  const raw_ptr<BrowserView> browser_view_;
  const int expected_bottom_inset_;
  int resize_count_ = 0;
  int max_error_ = 0;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

}  // namespace

// Tests that the contents keep filling the browser view while the bookmark bar
// animates in and out with "Always Show Toolbar in Full Screen". The top
// container is in a separate widget, and the browser view resizes when the top
// container does.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       ContentsFillBrowserViewWhileBookmarkBarAnimates) {
  ScopedAlwaysShowToolbar scoped_always_show(browser(), true);
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  RunScheduledLayouts();

  for (bool show : {true, false}) {
    SCOPED_TRACE(show ? "Show bookmark bar" : "Hide bookmark bar");
    const int top_container_height = browser_view->top_container()->height();
    ContentsBottomChecker checker(browser_view);
    chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);
    EXPECT_TRUE(browser_view->IsBookmarkBarAnimating());
    ui_test_utils::CheckWaiter(
        base::BindRepeating(&BrowserView::IsBookmarkBarAnimating,
                            base::Unretained(browser_view)),
        false, base::Seconds(3))
        .Wait();
    RunScheduledLayouts();

    EXPECT_NE(browser_view->top_container()->height(), top_container_height);
    EXPECT_GT(checker.resize_count(), 0);
    EXPECT_EQ(checker.max_error(), 0);
  }
}
