// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/i18n/test/scoped_rtl_for_testing.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/viz/common/frame_timing_details.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/widget/native_widget_mac.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using BrowserNativeWidgetMacInteractiveTest = InProcessBrowserTest;

@interface KeyEquivalentSpyView : NSView
@property(nonatomic) BOOL receivedKeyEquivalent;
@property(nonatomic) BOOL composing;
@end

@implementation KeyEquivalentSpyView
@synthesize receivedKeyEquivalent = _receivedKeyEquivalent;
@synthesize composing = _composing;

- (BOOL)acceptsFirstResponder {
  return YES;
}
- (BOOL)performKeyEquivalent:(NSEvent*)event {
  self.receivedKeyEquivalent = YES;
  // IME composition consumes the equivalent so it never bubbles. Matches
  // RenderWidgetHostViewCocoa returning YES while composing.
  return self.composing;
}
@end

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

// Child widgets (WebUI omnibox popup) use DefaultCommandDispatcherDelegate,
// which forwards reserved equivalents to the parent before the firstResponder.
// A composing firstResponder must not consume Cmd+T; others still reach it.
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacInteractiveTest,
                       ChildWindowKeyEquivalentDispatch) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  auto child_widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = BrowserView::GetBrowserViewForBrowser(browser())
                      ->GetWidget()
                      ->GetNativeView();
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  child_widget->Init(std::move(params));
  child_widget->Show();

  NSWindow* child_window = child_widget->GetNativeWindow().GetNativeNSWindow();
  KeyEquivalentSpyView* spy = [[KeyEquivalentSpyView alloc] init];
  [child_window.contentView addSubview:spy];
  ASSERT_TRUE([child_window makeFirstResponder:spy]);
  [child_window makeKeyWindow];

  auto cmd_event = [&](NSString* character, unsigned short key_code) {
    return [NSEvent keyEventWithType:NSEventTypeKeyDown
                            location:NSZeroPoint
                       modifierFlags:NSEventModifierFlagCommand
                           timestamp:0
                        windowNumber:child_window.windowNumber
                             context:nil
                          characters:character
         charactersIgnoringModifiers:character
                           isARepeat:NO
                             keyCode:key_code];
  };

  // US-ANSI T/L hardware keycodes. Numeric to avoid Carbon.h.
  constexpr unsigned short kKeyCodeT = 17;
  constexpr unsigned short kKeyCodeL = 37;

  TabStripModel* tabs = browser()->GetTabStripModel();
  ASSERT_EQ(1, tabs->count());

  // Korean IME composition: the firstResponder would consume the first Cmd+T
  // (no-op) and only a second press after composition ended would fire.
  spy.composing = YES;

  // Cmd+T is reserved: handled by the parent on the first press, not the
  // composing firstResponder.
  EXPECT_TRUE([child_window performKeyEquivalent:cmd_event(@"t", kKeyCodeT)]);
  EXPECT_EQ(2, tabs->count());
  EXPECT_FALSE(spy.receivedKeyEquivalent);

  // Cmd+L is not reserved: composing firstResponder still consumes it.
  EXPECT_TRUE([child_window performKeyEquivalent:cmd_event(@"l", kKeyCodeL)]);
  EXPECT_TRUE(spy.receivedKeyEquivalent);
  EXPECT_EQ(2, tabs->count());
}

namespace {

struct GlassViews {
  NSView* glass_view = nil;
  NSView* tint_view = nil;
  NSView* opaque_view = nil;
};

GlassViews GetGlassViews(NSView* content_view) {
  Class background_view_class = NSClassFromString(@"GlassFrameBackgroundView");
  Class opaque_view_class = NSClassFromString(@"OpaqueFrameBackgroundView");
  NSView* glass_view = nil;
  NSView* opaque_view = nil;
  for (NSView* subview in content_view.subviews) {
    if ([subview isKindOfClass:background_view_class]) {
      glass_view = subview;
    } else if ([subview isKindOfClass:opaque_view_class]) {
      opaque_view = subview;
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
  return {glass_view, tint_view, opaque_view};
}

int GetGlassCornerPadding() {
  return GetLayoutConstant(LayoutConstant::kToolbarCornerRadius) * 2;
}

int GetExpectedTopChromeGlassHeight(BrowserView* browser_view) {
  int height = 0;
  if (browser_view->browser_widget() &&
      browser_view->browser_widget()->GetFrameView()) {
    height = browser_view->browser_widget()->GetFrameView()->GetTopInset(true);
  }
  const auto top_element_info = browser_view->GetFrameElementInfo();
  height += top_element_info.top_area_height();
  height += top_element_info.toolbar_preferred_height;
  return height;
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

  auto [glass_view, tint_view, opaque_view] = GetGlassViews(content_view);

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
  auto [glass_view_deactivated, tint_view_deactivated,
        opaque_view_deactivated] = GetGlassViews(content_view);

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

  auto [glass_view, tint_view, opaque_view] = GetGlassViews(content_view);

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

  auto [glass_view_locked, tint_view_locked, opaque_view_locked] =
      GetGlassViews(content_view);
  SkColor locked_color =
      get_color_from_views(tint_view_locked, glass_view_locked);
  EXPECT_EQ(SkColorSetA(locked_color, 255), SkColorSetA(active_color, 255));

  // Release the lock. The widget should now transition to inactive tint.
  lock.reset();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !widget->ShouldPaintAsActive(); }));

  EXPECT_FALSE(widget->ShouldPaintAsActive());
  auto [glass_view_unlocked, tint_view_unlocked, opaque_view_unlocked] =
      GetGlassViews(content_view);
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

  auto [glass_view, tint_view, opaque_view] = GetGlassViews(content_view);
  ASSERT_NE(glass_view, nil);
  ASSERT_NE(opaque_view, nil);

  const int expected_height = GetExpectedTopChromeGlassHeight(browser_view);

  EXPECT_EQ(NSHeight(glass_view.frame), expected_height);
  EXPECT_LT(NSHeight(glass_view.frame), NSHeight(content_view.bounds));
  EXPECT_EQ(NSMaxY(glass_view.frame), NSMaxY(content_view.bounds));
  EXPECT_EQ(NSWidth(glass_view.frame), NSWidth(content_view.bounds));

  EXPECT_EQ(NSMinX(opaque_view.frame), 0);
  EXPECT_EQ(NSMinY(opaque_view.frame), 0);
  EXPECT_EQ(NSWidth(opaque_view.frame), NSWidth(content_view.bounds));
  EXPECT_EQ(NSHeight(opaque_view.frame),
            NSHeight(content_view.bounds) - expected_height);
  EXPECT_TRUE(opaque_view.layer.opaque);
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
  auto [glass_view, tint_view, opaque_view] = GetGlassViews(content_view);
  EXPECT_NE(glass_view, nil);
  EXPECT_NE(opaque_view, nil);

  ui_test_utils::ToggleFullscreenModeAndWait(browser());

  ASSERT_TRUE(base::test::RunUntil([&]() { return [ns_window isOpaque]; }));
  auto [fs_glass_view, fs_tint_view, fs_opaque_view] =
      GetGlassViews(content_view);
  EXPECT_EQ(fs_glass_view, nil);
  EXPECT_EQ(fs_opaque_view, nil);
}

IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacGlassTest,
                       TabFullscreenWindowIsOpaque) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP() << "Glass frame feature is disabled.";
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* widget = browser_view->GetWidget();
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  NSView* content_view = [ns_window contentView];

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  EXPECT_FALSE([ns_window isOpaque]);
  auto [glass_view, tint_view, opaque_view] = GetGlassViews(content_view);
  EXPECT_NE(glass_view, nil);
  EXPECT_NE(opaque_view, nil);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetDelegate()->EnterFullscreenModeForTab(
      web_contents->GetPrimaryMainFrame(), {});

  // Wait for Cocoa window to become opaque in tab fullscreen
  ASSERT_TRUE(base::test::RunUntil([&]() { return [ns_window isOpaque]; }));
  auto [fs_glass_view, fs_tint_view, fs_opaque_view] =
      GetGlassViews(content_view);
  EXPECT_EQ(fs_glass_view, nil);
  EXPECT_EQ(fs_opaque_view, nil);
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
  auto [glass1, tint1, opaque1] = GetGlassViews(first_content_view);
  EXPECT_NE(glass1, nil);
  EXPECT_NE(opaque1, nil);

  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());
  GlassFrameService::GetInstance()->OnBrowserActivated(second_browser);
  ASSERT_TRUE(base::test::RunUntil([&]() { return [first_window isOpaque]; }));
  EXPECT_EQ(1.0, [[first_window backgroundColor] alphaComponent]);
  EXPECT_TRUE(first_view->GetWidget()->GetLayer()->fills_bounds_opaquely());
  auto [glass1_ineligible, tint1_ineligible, opaque1_ineligible] =
      GetGlassViews(first_content_view);
  EXPECT_EQ(glass1_ineligible, nil);
  EXPECT_EQ(opaque1_ineligible, nil);

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
  ASSERT_TRUE(base::test::RunUntil([&]() { return [second_window isOpaque]; }));
  EXPECT_EQ(1.0, [[second_window backgroundColor] alphaComponent]);
  auto [glass2_ineligible, tint2_ineligible, opaque2_ineligible] =
      GetGlassViews(second_content_view);
  EXPECT_EQ(glass2_ineligible, nil);
  EXPECT_EQ(opaque2_ineligible, nil);

  CloseBrowserSynchronously(third_browser);
  CloseBrowserSynchronously(second_browser);
}

IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacGlassTest,
                       MultipleProfilesTintOpacity) {
  if (!features::IsGlassFrameEnabled()) {
    GTEST_SKIP() << "Glass frame feature is disabled.";
  }

  auto get_tint_alpha = [](BrowserWindowInterface* target_browser) {
    BrowserView* view = BrowserView::GetBrowserViewForBrowser(target_browser);
    NSWindow* window = view->GetWidget()->GetNativeWindow().GetNativeNSWindow();
    auto [glass_view, tint_view, opaque_view] =
        GetGlassViews([window contentView]);
    CHECK(tint_view);
    CHECK(tint_view.layer.backgroundColor);
    NSColor* ns_color =
        [NSColor colorWithCGColor:tint_view.layer.backgroundColor];
    NSColor* srgb_color =
        [ns_color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    return srgb_color.alphaComponent;
  };

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const CGFloat single_profile_alpha = get_tint_alpha(browser());

  // Opening a second window in the same profile keeps the original opacity.
  BrowserWindowInterface* same_profile_browser =
      CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return GlassFrameService::GetInstance()->IsBrowserWindowEligible(
        same_profile_browser);
  }));
  EXPECT_NEAR(get_tint_alpha(same_profile_browser), single_profile_alpha,
              0.001);
  CloseBrowserSynchronously(same_profile_browser);
  ASSERT_TRUE(base::test::RunUntil([&] {
    return GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser());
  }));

  // Opening a window in a second profile increases the tint opacity.
  constexpr double kMultipleProfileOpacityIncrease = 0.05;
  ProfileManager* const profile_manager = g_browser_process->profile_manager();
  const base::FilePath new_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& second_profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);
  BrowserWindowInterface* second_profile_browser =
      CreateBrowser(&second_profile);
  ASSERT_TRUE(base::test::RunUntil([&] {
    return GlassFrameService::GetInstance()->IsBrowserWindowEligible(
        second_profile_browser);
  }));
  EXPECT_NEAR(get_tint_alpha(second_profile_browser),
              single_profile_alpha + kMultipleProfileOpacityIncrease, 0.001);

  // Switching back to the first profile window while the second profile window
  // is still open also uses the increased tint opacity.
  browser()->GetWindow()->Activate();
  ASSERT_TRUE(base::test::RunUntil([&] {
    return GlassFrameService::GetInstance()->IsBrowserWindowEligible(browser());
  }));
  EXPECT_NEAR(get_tint_alpha(browser()),
              single_profile_alpha + kMultipleProfileOpacityIncrease, 0.001);

  // Closing the second profile window (while it is in the background and
  // ineligible) restores the original tint opacity on the remaining active
  // window.
  CloseBrowserSynchronously(second_profile_browser);
  EXPECT_NEAR(get_tint_alpha(browser()), single_profile_alpha, 0.001);
}

class BrowserNativeWidgetMacVerticalTabsGlassTest
    : public InProcessBrowserTest {
 public:
  BrowserNativeWidgetMacVerticalTabsGlassTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlassFrame);
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

  auto [glass_view, tint_view, opaque_view] = GetGlassViews(content_view);
  ASSERT_NE(glass_view, nil);
  ASSERT_NE(opaque_view, nil);

  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser());
  ASSERT_NE(controller, nullptr);

  const int corner_padding = GetGlassCornerPadding();
  const int expected_uncollapsed_width =
      controller->GetUncollapsedWidth() + corner_padding;

  EXPECT_EQ(NSHeight(glass_view.frame), NSHeight(content_view.bounds));
  EXPECT_EQ(NSWidth(glass_view.frame), expected_uncollapsed_width);
  EXPECT_LT(NSWidth(glass_view.frame), NSWidth(content_view.bounds));

  EXPECT_EQ(NSMinX(opaque_view.frame), expected_uncollapsed_width);
  EXPECT_EQ(NSMinY(opaque_view.frame), 0);
  EXPECT_EQ(NSWidth(opaque_view.frame),
            NSWidth(content_view.bounds) - expected_uncollapsed_width);
  EXPECT_EQ(NSHeight(opaque_view.frame), NSHeight(content_view.bounds));
  EXPECT_TRUE(opaque_view.layer.opaque);
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

  auto [glass_view, tint_view, opaque_view] = GetGlassViews(content_view);
  ASSERT_NE(glass_view, nil);
  ASSERT_NE(opaque_view, nil);

  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser());
  ASSERT_NE(controller, nullptr);

  const int corner_padding = GetGlassCornerPadding();
  const int expected_uncollapsed_width =
      controller->GetUncollapsedWidth() + corner_padding;

  // Initially at settled uncollapsed width.
  EXPECT_EQ(NSWidth(glass_view.frame), expected_uncollapsed_width);
  EXPECT_EQ(NSMinX(opaque_view.frame), expected_uncollapsed_width);
  EXPECT_EQ(NSWidth(opaque_view.frame),
            NSWidth(content_view.bounds) - expected_uncollapsed_width);

  // During drag resizing, glass view expands up to the maximum possible
  // vertical tab strip width.
  const int expected_resizing_width =
      tabs::kVerticalTabStripUncollapsedMaxWidth + corner_padding;
  controller->SetIsResizing(true);
  EXPECT_EQ(NSWidth(glass_view.frame), expected_resizing_width);
  EXPECT_EQ(NSMinX(opaque_view.frame), expected_resizing_width);
  EXPECT_EQ(NSWidth(opaque_view.frame),
            NSWidth(content_view.bounds) - expected_resizing_width);

  // When resizing ends, glass view returns to settled width.
  controller->SetIsResizing(false);
  EXPECT_EQ(NSWidth(glass_view.frame), expected_uncollapsed_width);
  EXPECT_EQ(NSMinX(opaque_view.frame), expected_uncollapsed_width);
  EXPECT_EQ(NSWidth(opaque_view.frame),
            NSWidth(content_view.bounds) - expected_uncollapsed_width);
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

  auto [glass_view, tint_view, opaque_view] = GetGlassViews(content_view);
  ASSERT_NE(glass_view, nil);
  ASSERT_NE(opaque_view, nil);

  auto* const controller =
      tabs::VerticalTabStripStateController::From(browser());
  ASSERT_NE(controller, nullptr);

  const int corner_padding = GetGlassCornerPadding();
  const int expected_collapsed_width =
      tabs::kVerticalTabStripCollapsedWidth + corner_padding;
  const int expected_uncollapsed_width =
      controller->GetUncollapsedWidth() + corner_padding;

  // Toggle to collapsed state.
  controller->RequestCollapse(true);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return controller->IsCollapsed(); }));
  EXPECT_EQ(NSWidth(glass_view.frame), expected_collapsed_width);
  EXPECT_EQ(NSMinX(opaque_view.frame), expected_collapsed_width);
  EXPECT_EQ(NSWidth(opaque_view.frame),
            NSWidth(content_view.bounds) - expected_collapsed_width);

  // Toggle back to expanded / uncollapsed state.
  controller->RequestCollapse(false);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !controller->IsCollapsed(); }));
  EXPECT_EQ(NSWidth(glass_view.frame), expected_uncollapsed_width);
  EXPECT_EQ(NSMinX(opaque_view.frame), expected_uncollapsed_width);
  EXPECT_EQ(NSWidth(opaque_view.frame),
            NSWidth(content_view.bounds) - expected_uncollapsed_width);
}

class BrowserNativeWidgetMacDeferInputContextTestBase
    : public InProcessBrowserTest {
 protected:
  explicit BrowserNativeWidgetMacDeferInputContextTestBase(bool enabled) {
    feature_list_.InitWithFeatureStates(
        {{features::kMacDeferInputContextUntilFirstPaint, enabled},
         {features::kWebUILocationBar, false},
         {features::kWebUIToolbar, false}});
  }

  // Creates a browser window with one tab showing `url`. The window is not
  // shown.
  BrowserWindowInterface* CreateUnshownBrowser(
      const GURL& url = GURL(url::kAboutBlankURL)) {
    BrowserWindowInterface* new_browser = CreateBrowserWindow(
        BrowserWindowCreateParams(browser()->GetProfile(),
                                  /*from_user_gesture=*/true));
    chrome::AddSelectedTabWithURL(new_browser, url,
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    return new_browser;
  }

  static views::Widget* GetWidget(BrowserWindowInterface* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)->GetWidget();
  }

  static BridgedContentView* GetContentView(BrowserWindowInterface* browser) {
    return views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
               GetWidget(browser)->GetNativeWindow())
        ->GetInProcessNSWindowBridge()
        ->ns_view();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class BrowserNativeWidgetMacDeferInputContextTest
    : public BrowserNativeWidgetMacDeferInputContextTestBase {
 public:
  BrowserNativeWidgetMacDeferInputContextTest()
      : BrowserNativeWidgetMacDeferInputContextTestBase(/*enabled=*/true) {}
};

class BrowserNativeWidgetMacDeferInputContextDisabledTest
    : public BrowserNativeWidgetMacDeferInputContextTestBase {
 public:
  BrowserNativeWidgetMacDeferInputContextDisabledTest()
      : BrowserNativeWidgetMacDeferInputContextTestBase(/*enabled=*/false) {}
};

// With the feature disabled, a new browser window has its input context from
// the start.
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacDeferInputContextDisabledTest,
                       NewWindowDoesNotDeferInputContext) {
  BridgedContentView* content_view = GetContentView(CreateUnshownBrowser());
  ASSERT_TRUE(content_view);
  EXPECT_FALSE(content_view.deferInputContextActivation);
}

// Closing a window before its first frame is presented does not crash.
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacDeferInputContextTest,
                       CloseBeforeFirstFrame) {
  BrowserWindowInterface* new_browser = CreateUnshownBrowser();
  BridgedContentView* content_view = GetContentView(new_browser);
  ASSERT_TRUE(content_view);
  EXPECT_TRUE(content_view.deferInputContextActivation);

  CloseBrowserSynchronously(new_browser);
}

// A new browser window opened on the New Tab Page focuses its location bar, as
// Cmd+N does. It holds back its input context until its first frame is
// presented, then ends the deferral in a task posted after that frame, which
// activates the input method for the location bar. Typing then reaches it.
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacDeferInputContextTest,
                       NewWindowDefersInputContextUntilFirstFrame) {
  constexpr char kDuration[] = "InputMethod.Mac.DeferredActivation.Duration";
  // Let the initial window end its own deferral, so only the new window
  // records below.
  BridgedContentView* initial_view = GetContentView(browser());
  ASSERT_TRUE(initial_view);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !initial_view.deferInputContextActivation; }));
  base::HistogramTester histograms;

  BrowserWindowInterface* new_browser =
      CreateUnshownBrowser(GURL(chrome::kChromeUINewTabURL));
  views::Widget* widget = GetWidget(new_browser);
  BridgedContentView* content_view = GetContentView(new_browser);
  ASSERT_TRUE(content_view);

  // The deferral starts when the widget is created, before it is shown.
  EXPECT_TRUE(content_view.deferInputContextActivation);
  EXPECT_FALSE(content_view.inputContext);

  // BrowserNativeWidgetMac asked for the first frame's presentation before
  // this callback, so its callback has run by now. It only posts the end of
  // the deferral, so the input method is not activated inside the
  // compositor's presentation callbacks.
  base::RunLoop presented;
  widget->GetCompositor()->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindLambdaForTesting(
          [&](const viz::FrameTimingDetails& frame_timing_details) {
            EXPECT_TRUE(content_view.deferInputContextActivation);
            EXPECT_NE(
                static_cast<id>(content_view),
                static_cast<id>(NSTextInputContext.currentInputContext.client));
            presented.Quit();
          }));
  // Make the window key as it shows, so the location bar takes focus before
  // the first frame and the end of the deferral activates its input method.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(new_browser));
  presented.Run();
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return !content_view.deferInputContextActivation; }));

  // Ending the deferral, not a later focus change, made the location bar's
  // input context the current one.
  ASSERT_TRUE(ui_test_utils::IsViewFocused(new_browser, VIEW_ID_OMNIBOX));
  ASSERT_TRUE(content_view.inputContext);
  EXPECT_EQ(content_view.inputContext, NSTextInputContext.currentInputContext);
  histograms.ExpectTotalCount(kDuration, 1);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(new_browser, ui::VKEY_A,
                                              /*control=*/false,
                                              /*shift=*/false, /*alt=*/false,
                                              /*command=*/false));
  EXPECT_EQ(u"a", BrowserView::GetBrowserViewForBrowser(new_browser)
                      ->GetLocationBarView()
                      ->GetOmniboxView()
                      ->GetText());
}

// Opening a new window does not defer the input context of a window that is
// already open.
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacDeferInputContextTest,
                       NewWindowDoesNotDeferExistingWindow) {
  BridgedContentView* existing_view = GetContentView(browser());
  ASSERT_TRUE(existing_view);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !existing_view.deferInputContextActivation; }));

  BridgedContentView* new_view = GetContentView(CreateUnshownBrowser());
  ASSERT_TRUE(new_view);
  EXPECT_TRUE(new_view.deferInputContextActivation);
  EXPECT_FALSE(existing_view.deferInputContextActivation);
}

// Two windows opened back to back both end their deferral, and typing reaches
// the location bar of the one in front.
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacDeferInputContextTest,
                       TwoWindowsOpenedBackToBack) {
  BrowserWindowInterface* first = CreateUnshownBrowser();
  BrowserWindowInterface* second = CreateUnshownBrowser();
  BridgedContentView* first_view = GetContentView(first);
  BridgedContentView* second_view = GetContentView(second);
  ASSERT_TRUE(first_view);
  ASSERT_TRUE(second_view);
  EXPECT_TRUE(first_view.deferInputContextActivation);
  EXPECT_TRUE(second_view.deferInputContextActivation);

  first->GetWindow()->Show();
  second->GetWindow()->Show();
  EXPECT_TRUE(base::test::RunUntil([&] {
    return !first_view.deferInputContextActivation &&
           !second_view.deferInputContextActivation;
  }));

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(second));
  ui_test_utils::FocusView(second, VIEW_ID_OMNIBOX);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(second, ui::VKEY_A,
                                              /*control=*/false,
                                              /*shift=*/false, /*alt=*/false,
                                              /*command=*/false));
  EXPECT_EQ(u"a", BrowserView::GetBrowserViewForBrowser(second)
                      ->GetLocationBarView()
                      ->GetOmniboxView()
                      ->GetText());
}

// A key press that arrives while the input context is deferred ends the
// deferral and reaches the location bar, so the first typed key is not lost.
IN_PROC_BROWSER_TEST_F(BrowserNativeWidgetMacDeferInputContextTest,
                       KeyPressDuringDeferralReachesLocationBar) {
  BridgedContentView* content_view = GetContentView(browser());
  ASSERT_TRUE(content_view);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !content_view.deferInputContextActivation; }));
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::FocusView(browser(), VIEW_ID_OMNIBOX);

  // Arm the deferral again, as for a new window before its first frame.
  content_view.deferInputContextActivation = YES;
  ASSERT_FALSE(content_view.inputContext);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A,
                                              /*control=*/false,
                                              /*shift=*/false, /*alt=*/false,
                                              /*command=*/false));
  EXPECT_FALSE(content_view.deferInputContextActivation);
  EXPECT_TRUE(content_view.inputContext);
  EXPECT_EQ(u"a", BrowserView::GetBrowserViewForBrowser(browser())
                      ->GetLocationBarView()
                      ->GetOmniboxView()
                      ->GetText());
}
