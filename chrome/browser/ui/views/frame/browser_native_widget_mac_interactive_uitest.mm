// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_native_widget_mac.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"

using BrowserNativeWidgetMacInteractiveTest = InProcessBrowserTest;

// Tests that closing the browser immediately after toggling fullscreen doesn't
// crash and successfully closes the browser.
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacInteractiveTest,
                       CloseDuringFullscreenTransition) {
  // Ensure the window is active.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Trigger fullscreen. This starts the asynchronous transition.
  chrome::ToggleFullscreenMode(browser());

  // Immediately try to close the browser.
  // We use BrowserDestroyedObserver to wait for it to be fully destroyed.
  ui_test_utils::BrowserDestroyedObserver observer(browser());
  chrome::CloseWindow(browser());

  // Wait for the browser to be destroyed. If there is a crash or if it hangs,
  // the test will fail/timeout.
  observer.Wait();
}

namespace {

struct GlassViews {
  NSView* glass_view = nil;
  NSView* tint_view = nil;
};

GlassViews GetGlassViews(NSView* content_view) {
  Class background_view_class = NSClassFromString(@"GlassFrameBackgroundView");
  NSView* glass_view = nil;
  for (NSView* subview in content_view.subviews) {
    if ([subview isKindOfClass:background_view_class]) {
      glass_view = subview;
      break;
    }
  }

  NSView* tint_view = nil;
  if (glass_view) {
    for (NSView* subview in glass_view.subviews) {
      if ([NSStringFromClass([subview class]) isEqualToString:@"NSView"]) {
        tint_view = subview;
        break;
      }
    }
  }
  return {glass_view, tint_view};
}

}  // namespace

class TestNativeWidgetMac : public views::NativeWidgetMac {
 public:
  static views::NativeWidgetMacNSWindowHost* GetHost(
      views::NativeWidgetMac* widget) {
    return static_cast<TestNativeWidgetMac*>(widget)->GetNSWindowHost();
  }
};

class BrowserNativeWidgetMacGlassTest : public InProcessBrowserTest {
 public:
  BrowserNativeWidgetMacGlassTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlassFrame);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacGlassTest, ActiveInactiveTintOpacity) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP() << "Glass frame is not supported on this macOS version.";
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* widget = browser_view->GetWidget();
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  NSView* content_view = [ns_window contentView];

  // Bring browser to front to ensure it is active.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(widget->IsActive());

  auto [glass_view, tint_view] = GetGlassViews(content_view);

  // Get active and inactive colors.
  const ui::ColorProvider* color_provider = browser_view->GetColorProvider();
  SkColor active_color = color_provider->GetColor(ui::kColorFrameActive);
  SkColor inactive_color = color_provider->GetColor(ui::kColorFrameInactive);

  auto get_sk_color = [](CGColorRef cg_color) {
    if (!cg_color) {
      return SK_ColorTRANSPARENT;
    }
    NSColor* ns_color = [NSColor colorWithCGColor:cg_color];
    NSColor* srgb_color = [ns_color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    return SkColorSetARGB(
        static_cast<U8CPU>(srgb_color.alphaComponent * 255),
        static_cast<U8CPU>(srgb_color.redComponent * 255),
        static_cast<U8CPU>(srgb_color.greenComponent * 255),
        static_cast<U8CPU>(srgb_color.blueComponent * 255));
  };

  auto get_color_from_views = [&](NSView* t_view, NSView* g_view) {
    if (t_view) {
      return get_sk_color(t_view.layer.backgroundColor);
    }
    CHECK(g_view);
    NSColor* t_color = [(id)g_view tintColor];
    NSColor* srgb_color = [t_color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    return SkColorSetARGB(
        static_cast<U8CPU>(srgb_color.alphaComponent * 255),
        static_cast<U8CPU>(srgb_color.redComponent * 255),
        static_cast<U8CPU>(srgb_color.greenComponent * 255),
        static_cast<U8CPU>(srgb_color.blueComponent * 255));
  };

  // Initially active.
  SkColor initial_color = get_color_from_views(tint_view, glass_view);
  EXPECT_EQ(SkColorSetA(initial_color, 255), SkColorSetA(active_color, 255));

  // Deactivate the widget by simulating native key status change.
  views::NativeWidgetMac* native_widget =
      static_cast<views::NativeWidgetMac*>(widget->native_widget());
  static_cast<remote_cocoa::mojom::NativeWidgetNSWindowHost*>(
      TestNativeWidgetMac::GetHost(native_widget))
      ->OnWindowKeyStatusChanged(/*is_key=*/false,
                                 /*is_content_first_responder=*/false,
                                 /*full_keyboard_access_enabled=*/false);
  base::RunLoop().RunUntilIdle();

  // Find views again.
  auto [glass_view_deactivated, tint_view_deactivated] =
      GetGlassViews(content_view);

  SkColor deactivated_color =
      get_color_from_views(tint_view_deactivated, glass_view_deactivated);
  EXPECT_EQ(SkColorSetA(deactivated_color, 255), SkColorSetA(inactive_color, 255));
}
