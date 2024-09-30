// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_mac.h"

#import <Cocoa/Cocoa.h>

#include <tuple>

#include "base/apple/foundation_util.h"
#import "base/mac/mac_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_interactive_uitest_utils.h"

class ScopedAlwaysShowToolbar {
 public:
  ScopedAlwaysShowToolbar(Browser* browser, bool always_show) {
    prefs_ = browser->profile()->GetPrefs();
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
  ImmersiveModeControllerMacInteractiveTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kImmersiveFullscreen);
  }

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
    return browser()->window()->GetNativeWindow().GetNativeNSWindow();
  }

  // Creates a new widget as a child of the first browser window and brings it
  // onscreen.
  void CreateAndShowWidgetOnFirstBrowserWindow() {
    NSUInteger starting_child_window_count =
        browser_window().childWindows.count;

    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(100, 100, 200, 200);
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    params.parent = browser_view->GetWidget()->GetNativeView();
    params.z_order = ui::ZOrderLevel::kNormal;

    params.delegate = new views::WidgetDelegateView();

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
    this->second_browser_ = CreateBrowser(browser()->profile());
  }

  // Makes the second browser window the active window and ensures it's on the
  // active space.
  void ActivateSecondBrowserWindow() {
    views::test::PropertyWaiter activate_waiter(
        base::BindRepeating(&ui::BaseWindow::IsActive,
                            base::Unretained(second_browser_->window())),
        true);
    second_browser_->window()->Activate();
    EXPECT_TRUE(activate_waiter.Wait());

    views::test::PropertyWaiter active_space_waiter(
        base::BindRepeating(&ImmersiveModeControllerMacInteractiveTest::
                                SecondBrowserWindowIsOnTheActiveSpace,
                            base::Unretained(this)),
        true);
    EXPECT_TRUE(active_space_waiter.Wait());
  }

  bool SecondBrowserWindowIsOnTheActiveSpace() {
    return second_browser_->window()
        ->GetNativeWindow()
        .GetNativeNSWindow()
        .isOnActiveSpace;
  }

  bool WidgetIsVisible() {
    return widget_->GetNativeWindow().GetNativeNSWindow().isVisible;
  }

  void CleanUp() {
    // Let the test harness free the second browser.
    second_browser_ = nullptr;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<Browser> second_browser_ = nullptr;
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
      browser()->exclusive_access_manager()->fullscreen_controller();

  EXPECT_TRUE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget),
            overlay_widget_content_view);

  // Only on macOS 13 and higher will the contentView no longer live in the
  // window.
  if (base::mac::MacOSMajorVersion() >= 13) {
    EXPECT_NE([overlay_widget_window contentView], overlay_widget_content_view);
  }

  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget), nullptr);
  EXPECT_EQ([overlay_widget_window contentView], overlay_widget_content_view);
}

// Tests that minimum content offset is nonzero iff the find bar is shown and
// "Always Show Toolbar in Full Screen" is off.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       MinimumContentOffset) {
  DisableFindBarAnimationsDuringTesting(true);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ImmersiveModeController* controller =
      browser_view->immersive_mode_controller();
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
  DisableFindBarAnimationsDuringTesting(false);
}

IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       ExtraInfobarOffset) {
  ScopedAlwaysShowToolbar scoped_always_show(browser(), false);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ImmersiveModeControllerMac* controller =
      reinterpret_cast<ImmersiveModeControllerMac*>(
          browser_view->immersive_mode_controller());
  controller->SetEnabled(true);

  controller->OnImmersiveModeMenuBarRevealChanged(0);
  controller->OnAutohidingMenuBarHeightChanged(0);
  EXPECT_EQ(controller->GetExtraInfobarOffset(), 0);

  controller->OnImmersiveModeMenuBarRevealChanged(0.5);
  int half_revealed = controller->GetExtraInfobarOffset();
  EXPECT_GT(half_revealed, 0);

  controller->OnImmersiveModeMenuBarRevealChanged(1);
  int revealed = controller->GetExtraInfobarOffset();
  EXPECT_EQ(revealed, half_revealed * 2);

  // Now with non-zero menubar.
  controller->OnAutohidingMenuBarHeightChanged(30);
  EXPECT_EQ(controller->GetExtraInfobarOffset(), revealed + 30);

  controller->OnImmersiveModeMenuBarRevealChanged(0.5);
  EXPECT_EQ(controller->GetExtraInfobarOffset(), half_revealed + 15);

  controller->OnImmersiveModeMenuBarRevealChanged(0);
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
  // fullscreen space in macOS 13+. http://crbug.com/1454606 stops the space
  // switch from happening on macOS 13+.
  HideWidget();

  // The space switch happens out of process and asynchronously. We want to make
  // sure the space switch doesn't happen, which means waiting for a bit. In the
  // expected case we will trip PropertyWaiter timeout. If this ends up being
  // flakey we need to extend the timeout or find a different approach for
  // testing.
  views::test::PropertyWaiter activate_waiter(
      base::BindRepeating(&ui::BaseWindow::IsActive,
                          base::Unretained(browser()->window())),
      true);
  EXPECT_FALSE(activate_waiter.Wait());

  // We should still be on the original space (no sudden space change).
  EXPECT_TRUE(SecondBrowserWindowIsOnTheActiveSpace());

  CleanUp();
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
// http://crbug.com/1454606. If this test starts failing, the workaround for
// issue 1454606 will need to be revisited.
// TODO(http://crbug.com/1454606): Remove this test when Apple fixes FB13529873.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       RebuildOrderingGroup) {
  // This test only applies to macOS 13 or greater.
  if (@available(macOS 13, *)) {
  } else {
    return;
  }

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
  EXPECT_TRUE(testWindow.isVisible);

  // Create a popup window and make it a child of the test window.
  NSWindow* popupWindow =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 50, 50)
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  popupWindow.releasedWhenClosed = NO;
  popupWindow.backgroundColor = NSColor.greenColor;
  [testWindow addChildWindow:popupWindow ordered:NSWindowAbove];
  EXPECT_TRUE(popupWindow.isVisible);

  // Reset the ordering group rebuilt flag and make sure it get set during
  // `-orderOut:`.
  testWindow->_orderingGroupRebuilt = NO;
  [popupWindow orderOut:nil];
  EXPECT_TRUE(testWindow->_orderingGroupRebuilt);

  // Re-add the popup window as child of the test window.
  [testWindow addChildWindow:popupWindow ordered:NSWindowAbove];
  EXPECT_TRUE(popupWindow.isVisible);

  // Reset the ordering group rebuilt flag and make sure it get set during
  // `-close`.
  testWindow->_orderingGroupRebuilt = NO;
  [popupWindow close];
  EXPECT_TRUE(testWindow->_orderingGroupRebuilt);

  // Re-add the popup window as child of the test window, then ensure that the
  // ordering group is rebuilt when the test window removes the popup window.
  [testWindow addChildWindow:popupWindow ordered:NSWindowAbove];
  EXPECT_TRUE(popupWindow.isVisible);
  testWindow->_orderingGroupRebuilt = NO;
  [testWindow removeChildWindow:popupWindow];
  EXPECT_TRUE(testWindow->_orderingGroupRebuilt);

  // Cleanup
  [popupWindow close];
  [testWindow close];
}

IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       ContentFullscreenChildren) {
  DisableFindBarAnimationsDuringTesting(true);

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
  DisableFindBarAnimationsDuringTesting(false);
}
