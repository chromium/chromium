// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_mac.h"

#import <Cocoa/Cocoa.h>

#include <tuple>

#include "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#import "components/remote_cocoa/app_shim/browser_native_widget_window_mac.h"
#include "content/public/test/browser_test.h"
#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_interactive_uitest_utils.h"

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

  // Convenience function to get the BrowserNativeWidgetWindow from the browser
  // window.
  BrowserNativeWidgetWindow* browser_window() {
    return base::mac::ObjCCastStrict<BrowserNativeWidgetWindow>(
        browser()->window()->GetNativeWindow().GetNativeNSWindow());
  }

  bool BrowserWindowIsOnTheActiveSpace() {
    return [browser_window() isOnActiveSpace];
  }

  void CreateSecondBrowserWindow() {
    this->second_browser_ = CreateBrowser(browser()->profile());
  }

  bool SecondBrowserWindowIsOnTheActiveSpace() {
    NSWindow* second_browser_ns_window =
        base::mac::ObjCCastStrict<BrowserNativeWidgetWindow>(
            second_browser_->window()->GetNativeWindow().GetNativeNSWindow());

    return [second_browser_ns_window isOnActiveSpace];
  }

  void ToggleBrowserWindowFullscreen() {
    // The fullscreen change notification is sent asynchronously. The
    // notification is used to trigger changes in whether the shelf is auto
    // hidden.
    FullscreenNotificationObserver waiter(browser());
    chrome::ToggleFullscreenMode(browser());
    waiter.Wait();

    // Make sure the browser window has transitioned to the active space.
    views::test::PropertyWaiter active_space_waiter(
        base::BindRepeating(&ImmersiveModeControllerMacInteractiveTest::
                                BrowserWindowIsOnTheActiveSpace,
                            base::Unretained(this)),
        true);
    EXPECT_TRUE(active_space_waiter.Wait());
  }

  // Creates a new widget and brings it onscreen.
  void CreateAndShowWidget() {
    NSUInteger starting_child_window_count =
        [browser_window() childWindows].count;

    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(100, 100, 200, 200);
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    params.parent = browser_view->GetWidget()->GetNativeView();
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.z_order = ui::ZOrderLevel::kSecuritySurface;

    params.delegate = new views::WidgetDelegateView();

    widget_ = std::make_unique<views::Widget>();
    widget_->Init(std::move(params));

    views::View* root_view = widget_->GetRootView();
    root_view->SetBackground(views::CreateSolidBackground(SK_ColorRED));

    widget_->Show();

    // The browser should have one more child window.
    EXPECT_EQ(starting_child_window_count + 1,
              [browser_window() childWindows].count);
  }

  void HideWidget() { widget_->Hide(); }

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

  void ActivateBrowserWindow() {
    views::test::PropertyWaiter activate_waiter(
        base::BindRepeating(&ui::BaseWindow::IsActive,
                            base::Unretained(browser()->window())),
        true);
    browser()->window()->Activate();
    EXPECT_TRUE(activate_waiter.Wait());

    views::test::PropertyWaiter active_space_waiter(
        base::BindRepeating(&ImmersiveModeControllerMacInteractiveTest::
                                SecondBrowserWindowIsOnTheActiveSpace,
                            base::Unretained(this)),
        true);
    EXPECT_TRUE(active_space_waiter.Wait());
  }

  void ChangeWidgetOrdering() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    widget_->StackAboveWidget(browser_view->overlay_widget());
  }

  bool BrowserWindowHasDeferredChildWindowRemovals() {
    return [browser_window() hasDeferredChildWindowRemovalsForTesting];
  }

  bool WidgetWindowHasDeferredWindowOrderingCommands() {
    NativeWidgetMacNSWindow* widgetWindow =
        base::mac::ObjCCastStrict<NativeWidgetMacNSWindow>(
            widget_->GetNativeWindow().GetNativeNSWindow());

    return [widgetWindow hasDeferredChildWindowOrderingCommandsForTesting];
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
  ToggleBrowserWindowFullscreen();

  FullscreenController* fullscreen_controller =
      browser()->exclusive_access_manager()->fullscreen_controller();

  EXPECT_TRUE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget),
            overlay_widget_content_view);

  // Only on macOS 13 and higher will the contentView no longer live in the
  // window.
  if (base::mac::IsAtLeastOS13()) {
    EXPECT_NE([overlay_widget_window contentView], overlay_widget_content_view);
  }

  ToggleBrowserWindowFullscreen();

  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget), nullptr);
  EXPECT_EQ([overlay_widget_window contentView], overlay_widget_content_view);
}

// Tests that minimum content offset is nonzero iff the find bar is shown.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       MinimumContentOffset) {
  chrome::DisableFindBarAnimationsDuringTesting(true);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ImmersiveModeController* controller =
      browser_view->immersive_mode_controller();
  controller->SetEnabled(true);
  EXPECT_EQ(controller->GetMinimumContentOffset(), 0);

  views::NamedWidgetShownWaiter shown_waiter(
      views::test::AnyWidgetTestPasskey{}, "DropdownBarHost");
  chrome::Find(browser());
  std::ignore = shown_waiter.WaitIfNeededAndGet();
  EXPECT_GT(controller->GetMinimumContentOffset(), 0);

  chrome::CloseFind(browser());
  EXPECT_EQ(controller->GetMinimumContentOffset(), 0);

  chrome::DisableFindBarAnimationsDuringTesting(false);
}

// Tests that ordering a child window out on a fullscreen window when that
// window is not on the active space does not trigger a space switch.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       DeferChildWindowRemoval) {
  // Create a new browser window for later.
  CreateSecondBrowserWindow();

  // Move the original browser window into its own fullscreen space.
  ToggleBrowserWindowFullscreen();

  // Add a widget (and its child window) to the browser window.
  CreateAndShowWidget();

  // We're still in the fullscreen space and no widgets have gone away, so
  // there should be no pending child window removals.
  EXPECT_FALSE(BrowserWindowHasDeferredChildWindowRemovals());

  // Switch to the second browser window, which will take us out of the
  // fullscreen space.
  ActivateSecondBrowserWindow();

  // The animator that hides the "Press Fn(f) to exit fullscreen" widget
  // generates multiple widget reordering calls. On macOS, the only way to
  // reorder widgets (which live in child windows) is to remove them and
  // add them back, which triggers a Space switch. Test what happens when
  // we generate a reordering command.
  ChangeWidgetOrdering();

  // Rather than performing the reordering (remove/add) operation, the widget
  // should have cached those instructions (which it will replay later when
  // it's back on the active space).
  EXPECT_TRUE(WidgetWindowHasDeferredWindowOrderingCommands());

  // Remove the widget.
  HideWidget();

  // The widget's window should still exist as a deferred removal.
  EXPECT_TRUE(BrowserWindowHasDeferredChildWindowRemovals());
  EXPECT_TRUE(WidgetWindowHasDeferredWindowOrderingCommands());

  // We should also still be on the original space (no sudden space change).
  EXPECT_TRUE(SecondBrowserWindowIsOnTheActiveSpace());

  // Switch back to the fullscreened browser window, which will make it
  // visible on the active space and trigger the removal of the widget
  // child window.
  ActivateBrowserWindow();

  EXPECT_FALSE(BrowserWindowHasDeferredChildWindowRemovals());

  CleanUp();
}
