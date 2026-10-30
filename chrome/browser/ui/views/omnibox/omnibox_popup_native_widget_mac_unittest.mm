// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/omnibox/omnibox_popup_native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#import "base/apple/foundation_util.h"
#import "base/apple/scoped_objc_class_swizzler.h"
#include "chrome/test/views/chrome_views_test_base.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace {

// The last window whose NSWindow -toggleFullScreen: implementation ran.
NSWindow* __weak g_toggled_window = nil;

}  // namespace

// Swizzled into NSWindow to record which window AppKit would act on, without
// starting a real fullscreen transition.
@interface OmniboxPopupFullscreenRecorder : NSWindow
@end

@implementation OmniboxPopupFullscreenRecorder

- (void)toggleFullScreen:(id)sender {
  g_toggled_window = self;
}

@end

namespace {

NativeWidgetMacNSWindow* GetNSWindow(views::Widget* widget) {
  return base::apple::ObjCCastStrict<NativeWidgetMacNSWindow>(
      widget->GetNativeWindow().GetNativeNSWindow());
}

// Sends -toggleFullScreen: to `window` and returns the window whose NSWindow
// implementation received it.
NSWindow* ToggleFullScreen(NSWindow* window) {
  g_toggled_window = nil;
  base::apple::ScopedObjCClassSwizzler swizzler(
      [NSWindow class], [OmniboxPopupFullscreenRecorder class],
      @selector(toggleFullScreen:));
  [window toggleFullScreen:nil];
  return g_toggled_window;
}

class OmniboxPopupNativeWidgetMacTest : public ChromeViewsTestBase {
 protected:
  std::unique_ptr<views::Widget> CreateWindowWidget() {
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(100, 100, 400, 300);
    widget->Init(std::move(params));
    widget->Show();
    return widget;
  }

  // Creates a frameless child of `parent`. `popup` selects the omnibox popup
  // NSWindow; otherwise it is a plain NativeWidgetMacNSWindow, like the
  // immersive fullscreen overlay.
  std::unique_ptr<views::Widget> CreateChildWidget(views::Widget* parent,
                                                   bool popup) {
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.parent = parent->GetNativeView();
    params.bounds = gfx::Rect(110, 110, 200, 100);
    if (popup) {
      params.native_widget = CreateOmniboxPopupNativeWidget(widget.get());
    }
    widget->Init(std::move(params));
    widget->Show();
    return widget;
  }
};

// Ctrl+Cmd+F reaches the popup while it is key. It must toggle the browser
// window, since the borderless popup can't go fullscreen.
TEST_F(OmniboxPopupNativeWidgetMacTest, ToggleFullScreenTogglesParentWindow) {
  std::unique_ptr<views::Widget> browser = CreateWindowWidget();
  std::unique_ptr<views::Widget> popup =
      CreateChildWidget(browser.get(), /*popup=*/true);
  // An active popup keeps its parent painting as active, which makes the
  // parent its commandDispatchParent.
  auto paint_as_active_lock = popup->LockPaintAsActive();
  NativeWidgetMacNSWindow* popup_window = GetNSWindow(popup.get());
  ASSERT_TRUE([popup_window isKindOfClass:[OmniboxPopupNSWindow class]]);
  ASSERT_EQ(GetNSWindow(browser.get()), popup_window.commandDispatchParent);

  EXPECT_EQ(GetNSWindow(browser.get()), ToggleFullScreen(popup_window));
}

// In immersive fullscreen the popup is a child of the overlay window, which
// AppKit parents to its NSToolbarFullScreenWindow. The overlay's
// commandDispatchParentOverride points at the browser window, so Ctrl+Cmd+F
// must exit fullscreen on the browser window, not the toolbar window.
TEST_F(OmniboxPopupNativeWidgetMacTest,
       ToggleFullScreenFollowsCommandDispatchParentOverride) {
  std::unique_ptr<views::Widget> browser = CreateWindowWidget();
  // Stands in for AppKit's NSToolbarFullScreenWindow.
  std::unique_ptr<views::Widget> toolbar_window = CreateWindowWidget();
  std::unique_ptr<views::Widget> overlay =
      CreateChildWidget(toolbar_window.get(), /*popup=*/false);
  GetNSWindow(overlay.get()).commandDispatchParentOverride =
      GetNSWindow(browser.get());
  std::unique_ptr<views::Widget> popup =
      CreateChildWidget(overlay.get(), /*popup=*/true);
  auto paint_as_active_lock = popup->LockPaintAsActive();
  NativeWidgetMacNSWindow* popup_window = GetNSWindow(popup.get());
  ASSERT_EQ(GetNSWindow(overlay.get()), popup_window.commandDispatchParent);

  EXPECT_EQ(GetNSWindow(browser.get()), ToggleFullScreen(popup_window));
}

}  // namespace
