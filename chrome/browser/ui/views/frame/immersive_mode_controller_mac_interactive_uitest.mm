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
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
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
    return base::apple::ObjCCastStrict<BrowserNativeWidgetWindow>(
        browser()->window()->GetNativeWindow().GetNativeNSWindow());
  }

  void ToggleBrowserWindowFullscreen() {
    // The fullscreen change notification is sent asynchronously. The
    // notification is used to trigger changes in whether the shelf is auto
    // hidden.
    FullscreenNotificationObserver waiter(browser());
    chrome::ToggleFullscreenMode(browser());
    waiter.Wait();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
  if (base::mac::MacOSMajorVersion() >= 13) {
    EXPECT_NE([overlay_widget_window contentView], overlay_widget_content_view);
  }

  ToggleBrowserWindowFullscreen();

  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(GetMovedContentViewForWidget(overlay_widget), nullptr);
  EXPECT_EQ([overlay_widget_window contentView], overlay_widget_content_view);
}

// Tests that minimum content offset is nonzero iff the find bar is shown and
// "Always Show Toolbar in Full Screen" is off.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerMacInteractiveTest,
                       MinimumContentOffset) {
  chrome::DisableFindBarAnimationsDuringTesting(true);

  PrefService* prefs = browser()->profile()->GetPrefs();
  bool original_always_show = prefs->GetBoolean(prefs::kShowFullscreenToolbar);

  prefs->SetBoolean(prefs::kShowFullscreenToolbar, false);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ImmersiveModeController* controller =
      browser_view->immersive_mode_controller();
  controller->SetEnabled(true);
  EXPECT_EQ(controller->GetMinimumContentOffset(), 0);

  {
    views::NamedWidgetShownWaiter shown_waiter(
        views::test::AnyWidgetTestPasskey{}, "DropdownBarHost");
    chrome::Find(browser());
    std::ignore = shown_waiter.WaitIfNeededAndGet();
    EXPECT_GT(controller->GetMinimumContentOffset(), 0);
  }

  chrome::CloseFind(browser());
  EXPECT_EQ(controller->GetMinimumContentOffset(), 0);

  // Now, with "Always Show..." on
  prefs->SetBoolean(prefs::kShowFullscreenToolbar, true);
  {
    views::NamedWidgetShownWaiter shown_waiter(
        views::test::AnyWidgetTestPasskey{}, "DropdownBarHost");
    chrome::Find(browser());
    std::ignore = shown_waiter.WaitIfNeededAndGet();
    EXPECT_EQ(controller->GetMinimumContentOffset(), 0);
  }
  chrome::CloseFind(browser());
  EXPECT_EQ(controller->GetMinimumContentOffset(), 0);

  // Reset the pref.
  prefs->SetBoolean(prefs::kShowFullscreenToolbar, original_always_show);

  chrome::DisableFindBarAnimationsDuringTesting(false);
}
