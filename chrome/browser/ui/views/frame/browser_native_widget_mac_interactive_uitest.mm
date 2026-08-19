// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/widget/native_widget_mac.h"

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
      if ([subview isMemberOfClass:[NSView class]]) {
        tint_view = subview;
        break;
      }
    }
  }
  return {glass_view, tint_view};
}

int GetGlassCornerPadding() {
  return GetLayoutConstant(LayoutConstant::kToolbarCornerRadius) * 2;
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
    GTEST_SKIP() << "Glass frame feature is disabled.";
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

IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacGlassTest,
                       PaintAsActiveLockMaintainsActiveTint) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP() << "Glass frame feature is disabled.";
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
    NSColor* srgb_color =
        [ns_color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    return SkColorSetARGB(static_cast<U8CPU>(srgb_color.alphaComponent * 255),
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
    NSColor* srgb_color =
        [t_color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    return SkColorSetARGB(static_cast<U8CPU>(srgb_color.alphaComponent * 255),
                          static_cast<U8CPU>(srgb_color.redComponent * 255),
                          static_cast<U8CPU>(srgb_color.greenComponent * 255),
                          static_cast<U8CPU>(srgb_color.blueComponent * 255));
  };

  // Lock paint-as-active on the widget (e.g. as a child bubble/widget would).
  std::unique_ptr<views::Widget::PaintAsActiveLock> lock =
      widget->LockPaintAsActive();

  // Deactivate the native widget (simulating a child window becoming key).
  views::NativeWidgetMac* native_widget =
      static_cast<views::NativeWidgetMac*>(widget->native_widget());
  static_cast<remote_cocoa::mojom::NativeWidgetNSWindowHost*>(
      TestNativeWidgetMac::GetHost(native_widget))
      ->OnWindowKeyStatusChanged(/*is_key=*/false,
                                 /*is_content_first_responder=*/false,
                                 /*full_keyboard_access_enabled=*/false);
  ASSERT_TRUE(base::test::RunUntil([&]() { return !widget->IsActive(); }));

  // Widget is natively inactive, but ShouldPaintAsActive() is true due to the
  // lock.
  EXPECT_FALSE(widget->IsActive());
  EXPECT_TRUE(widget->ShouldPaintAsActive());

  auto [glass_view_locked, tint_view_locked] = GetGlassViews(content_view);
  SkColor locked_color =
      get_color_from_views(tint_view_locked, glass_view_locked);
  EXPECT_EQ(SkColorSetA(locked_color, 255), SkColorSetA(active_color, 255));

  // Release the lock. The widget should now transition to inactive tint.
  lock.reset();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !widget->ShouldPaintAsActive(); }));

  EXPECT_FALSE(widget->ShouldPaintAsActive());
  auto [glass_view_unlocked, tint_view_unlocked] = GetGlassViews(content_view);
  SkColor unlocked_color =
      get_color_from_views(tint_view_unlocked, glass_view_unlocked);
  EXPECT_EQ(SkColorSetA(unlocked_color, 255), SkColorSetA(inactive_color, 255));
}

IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacGlassTest,
                       GlassViewGeometryRestrictedToTopChrome) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP() << "Glass frame feature is disabled.";
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* widget = browser_view->GetWidget();
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  NSView* content_view = [ns_window contentView];

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  auto [glass_view, tint_view] = GetGlassViews(content_view);
  ASSERT_NE(glass_view, nil);

  const auto top_element_info = browser_view->GetFrameElementInfo();
  int expected_height = top_element_info.top_area_height() +
                        top_element_info.toolbar_preferred_height;

  EXPECT_EQ(NSHeight(glass_view.frame), expected_height);
  EXPECT_LT(NSHeight(glass_view.frame), NSHeight(content_view.bounds));
  EXPECT_EQ(NSMaxY(glass_view.frame), NSMaxY(content_view.bounds));
  EXPECT_EQ(NSWidth(glass_view.frame), NSWidth(content_view.bounds));
}

IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacGlassTest,
                       FullscreenWindowIsOpaque) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP() << "Glass frame feature is disabled.";
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* widget = browser_view->GetWidget();
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  NSView* content_view = [ns_window contentView];

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  EXPECT_FALSE([ns_window isOpaque]);
  auto [glass_view, tint_view] = GetGlassViews(content_view);
  EXPECT_NE(glass_view, nil);

  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  EXPECT_TRUE([ns_window isOpaque]);
  auto [fs_glass_view, fs_tint_view] = GetGlassViews(content_view);
  EXPECT_EQ(fs_glass_view, nil);
}

IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacGlassTest,
                       IneligibleBrowserWindowIsOpaque) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP() << "Glass frame feature is disabled.";
  }

  BrowserView* first_view = BrowserView::GetBrowserViewForBrowser(browser());
  NSWindow* first_window =
      first_view->GetWidget()->GetNativeWindow().GetNativeNSWindow();
  NSView* first_content_view = [first_window contentView];
  EXPECT_EQ(0.001, [[first_window backgroundColor] alphaComponent]);
  EXPECT_FALSE(first_view->GetWidget()->GetLayer()->fills_bounds_opaquely());
  auto [glass1, tint1] = GetGlassViews(first_content_view);
  EXPECT_NE(glass1, nil);

  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());
  GlassFrameService::GetInstance()->OnBrowserActivated(second_browser);
  EXPECT_EQ(1.0, [[first_window backgroundColor] alphaComponent]);
  EXPECT_TRUE(first_view->GetWidget()->GetLayer()->fills_bounds_opaquely());
  auto [glass1_ineligible, tint1_ineligible] =
      GetGlassViews(first_content_view);
  EXPECT_EQ(glass1_ineligible, nil);

  BrowserWindowInterface* third_browser =
      CreateBrowser(browser()->GetProfile());
  GlassFrameService::GetInstance()->OnBrowserActivated(third_browser);
  NSWindow* second_window =
      BrowserView::GetBrowserViewForBrowser(second_browser)
          ->GetWidget()
          ->GetNativeWindow()
          .GetNativeNSWindow();
  NSView* second_content_view = [second_window contentView];

  EXPECT_EQ(1.0, [[first_window backgroundColor] alphaComponent]);
  EXPECT_TRUE(first_view->GetWidget()->GetLayer()->fills_bounds_opaquely());
  EXPECT_TRUE([second_window isOpaque]);
  EXPECT_EQ(1.0, [[second_window backgroundColor] alphaComponent]);
  auto [glass2_ineligible, tint2_ineligible] =
      GetGlassViews(second_content_view);
  EXPECT_EQ(glass2_ineligible, nil);

  CloseBrowserSynchronously(third_browser);
  CloseBrowserSynchronously(second_browser);
}

class BrowserNativeWidgetMacVerticalTabsGlassTest
    : public InProcessBrowserTest {
 public:
  BrowserNativeWidgetMacVerticalTabsGlassTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlassFrame, tabs::kVerticalTabs}, {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    if (auto* controller =
            tabs::VerticalTabStripStateController::From(browser())) {
      controller->SetVerticalTabsEnabled(true);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacVerticalTabsGlassTest,
                       GlassViewGeometryRestrictedToVerticalTabsRegion) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP() << "Glass frame feature is disabled.";
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* widget = browser_view->GetWidget();
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  NSView* content_view = [ns_window contentView];

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  auto [glass_view, tint_view] = GetGlassViews(content_view);
  ASSERT_NE(glass_view, nil);

  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser());
  ASSERT_NE(controller, nullptr);

  const int corner_padding = GetGlassCornerPadding();
  const int expected_uncollapsed_width =
      controller->GetUncollapsedWidth() + corner_padding;

  EXPECT_EQ(NSHeight(glass_view.frame), NSHeight(content_view.bounds));
  EXPECT_EQ(NSWidth(glass_view.frame), expected_uncollapsed_width);
  EXPECT_LT(NSWidth(glass_view.frame), NSWidth(content_view.bounds));
}

IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacVerticalTabsGlassTest,
                       GlassViewFullWidthDuringResize) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP() << "Glass frame feature is disabled.";
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* widget = browser_view->GetWidget();
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  NSView* content_view = [ns_window contentView];

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  auto [glass_view, tint_view] = GetGlassViews(content_view);
  ASSERT_NE(glass_view, nil);

  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser());
  ASSERT_NE(controller, nullptr);

  const int corner_padding = GetGlassCornerPadding();
  const int expected_uncollapsed_width =
      controller->GetUncollapsedWidth() + corner_padding;

  // Initially at settled uncollapsed width.
  EXPECT_EQ(NSWidth(glass_view.frame), expected_uncollapsed_width);

  // During drag resizing, glass view expands up to the maximum possible
  // vertical tab strip width.
  const int expected_resizing_width =
      VerticalTabStripRegionView::kUncollapsedMaxWidth + corner_padding;
  controller->SetIsResizing(true);
  EXPECT_EQ(NSWidth(glass_view.frame), expected_resizing_width);

  // When resizing ends, glass view returns to settled width.
  controller->SetIsResizing(false);
  EXPECT_EQ(NSWidth(glass_view.frame), expected_uncollapsed_width);
}

IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacVerticalTabsGlassTest,
                       GlassViewExpandsOnCollapseStateToggle) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP() << "Glass frame feature is disabled.";
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* widget = browser_view->GetWidget();
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  NSView* content_view = [ns_window contentView];

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  auto [glass_view, tint_view] = GetGlassViews(content_view);
  ASSERT_NE(glass_view, nil);

  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser());
  ASSERT_NE(controller, nullptr);

  const int corner_padding = GetGlassCornerPadding();
  const int expected_collapsed_width =
      VerticalTabStripRegionView::kCollapsedWidth + corner_padding;
  const int expected_uncollapsed_width =
      controller->GetUncollapsedWidth() + corner_padding;

  // Toggle to collapsed state.
  controller->RequestCollapse(true);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return controller->IsCollapsed(); }));
  EXPECT_EQ(NSWidth(glass_view.frame), expected_collapsed_width);

  // Toggle back to expanded / uncollapsed state.
  controller->RequestCollapse(false);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !controller->IsCollapsed(); }));
  EXPECT_EQ(NSWidth(glass_view.frame), expected_uncollapsed_width);
}
