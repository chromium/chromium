// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/browser_native_widget_window_mac.h"

#include <AppKit/AppKit.h>

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#import "components/remote_cocoa/app_shim/features.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_host_helper.h"
#import "components/remote_cocoa/app_shim/window_move_loop.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/accelerated_widget_mac/ca_transaction_observer.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/display/screen.h"
#import "ui/gfx/mac/coordinate_conversion.h"

using NativeWidgetMacOverlayNSWindowTest = PlatformTest;

@interface NSWindow (PrivateTestAPI)
- (void)_setFrame:(NSRect)frameRect
    fromAdjustmentToScreen:(NSScreen*)screen
            anchorIfNeeded:(const CGPoint*)anchor
                   animate:(BOOL)animate;
- (void)_setFrameAfterMove:(NSRect)frameRect;
@end

@interface NSThemeFrame (PrivateTrafficLightsAPI)
- (void)setButtonRevealAmount:(double)amount;
@property(readonly) NSView* closeButton;
@property(readonly) NSView* minimizeButton;
@property(readonly) NSView* zoomButton;
@end

// Test that private NSThemeFrame traffic light methods can be used to always
// show the traffic lights.
TEST(BrowserNativeWidgetWindowTest, AlwaysShowTrafficLights) {
  BrowserNativeWidgetWindow* browser_window = [[BrowserNativeWidgetWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  NSThemeFrame* theme_frame = base::apple::ObjCCastStrict<NSThemeFrame>(
      browser_window.contentView.superview);

  // Make sure the `NSThemeFrame` responds to the private API we are declaring.
  ASSERT_TRUE([theme_frame respondsToSelector:@selector(closeButton)]);
  ASSERT_TRUE([theme_frame respondsToSelector:@selector(minimizeButton)]);
  ASSERT_TRUE([theme_frame respondsToSelector:@selector(zoomButton)]);
  ASSERT_TRUE(
      [theme_frame respondsToSelector:@selector(setButtonRevealAmount:)]);

  // Ensure the traffic lights are visible.
  EXPECT_EQ(theme_frame.closeButton.alphaValue, 1.0);
  EXPECT_EQ(theme_frame.minimizeButton.alphaValue, 1.0);
  EXPECT_EQ(theme_frame.zoomButton.alphaValue, 1.0);

  // Hide the traffic lights.
  [theme_frame setButtonRevealAmount:0];
  EXPECT_EQ(theme_frame.closeButton.alphaValue, 0.0);
  EXPECT_EQ(theme_frame.minimizeButton.alphaValue, 0.0);
  EXPECT_EQ(theme_frame.zoomButton.alphaValue, 0.0);

  // Always show the traffic lights.
  [browser_window setAlwaysShowTrafficLights:YES];
  EXPECT_EQ(theme_frame.closeButton.alphaValue, 1.0);
  EXPECT_EQ(theme_frame.minimizeButton.alphaValue, 1.0);
  EXPECT_EQ(theme_frame.zoomButton.alphaValue, 1.0);

  // Try to hide the traffic lights again, they should remain visible.
  [theme_frame setButtonRevealAmount:0.0];
  EXPECT_EQ(theme_frame.closeButton.alphaValue, 1.0);
  EXPECT_EQ(theme_frame.minimizeButton.alphaValue, 1.0);
  EXPECT_EQ(theme_frame.zoomButton.alphaValue, 1.0);

  // Remove the always show pin, the traffic lights should be able hide again.
  [browser_window setAlwaysShowTrafficLights:NO];

  // Subsequent calls to `-setButtonRevealAmount:` with the same parameter value
  // will have no effect. Call with a temporary value, then with the desired
  // value.
  [theme_frame setButtonRevealAmount:1.0];
  [theme_frame setButtonRevealAmount:0];
  EXPECT_EQ(theme_frame.closeButton.alphaValue, 0.0);
  EXPECT_EQ(theme_frame.minimizeButton.alphaValue, 0.0);
  EXPECT_EQ(theme_frame.zoomButton.alphaValue, 0.0);
}

// Test that a window with enforceNeverMadeVisible cannot be ordered in.
// This prevents external frameworks (e.g., AuthenticationServicesCore)
// from making the invisible proxy window visible as a side effect of
// presenting modal dialogs. See https://crbug.com/325931972.
TEST(NativeWidgetMacNSWindowTest, EnforceNeverMadeVisibleBlocksOrderIn) {
  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];

  [window enforceNeverMadeVisible];
  EXPECT_FALSE([window isVisible]);

  // orderWindow:NSWindowAbove should be blocked.
  [window orderWindow:NSWindowAbove relativeTo:0];
  EXPECT_FALSE([window isVisible]);

  // orderWindow:NSWindowBelow should also be blocked.
  [window orderWindow:NSWindowBelow relativeTo:0];
  EXPECT_FALSE([window isVisible]);

  // orderWindow:NSWindowOut should still work (ordering out is fine).
  [window orderWindow:NSWindowOut relativeTo:0];
  EXPECT_FALSE([window isVisible]);
}

namespace {

class DummyNativeWidgetNSWindowHost
    : public remote_cocoa::mojom::NativeWidgetNSWindowHost {
 public:
  DummyNativeWidgetNSWindowHost() = default;
  ~DummyNativeWidgetNSWindowHost() override = default;

 private:
  void OnVisibilityChanged(bool visible) override {}
  void OnSpaceActivationChanged(bool is_space_active) override {}
  void OnWindowNativeThemeChanged() override {}
  void OnViewSizeChanged(const gfx::Size& new_size) override {}
  void SetKeyboardAccessible(bool enabled) override {}
  void OnIsFirstResponderChanged(bool is_first_responder) override {}
  void OnMouseCaptureActiveChanged(bool capture_is_active) override {}
  void OnScrollEvent(std::unique_ptr<ui::Event> event) override {}
  void OnMouseEvent(std::unique_ptr<ui::Event> event) override {}
  void OnGestureEvent(std::unique_ptr<ui::Event> event) override {}
  void OnWindowGeometryChanged(
      const gfx::Rect& window_bounds_in_screen_dips,
      const gfx::Rect& content_bounds_in_screen_dips) override {}
  void OnWindowWillMove() override {}
  void OnWindowDidEndMove() override {}
  void OnWindowWillStartLiveResize() override {}
  void OnWindowDidEndLiveResize() override {}
  void OnWindowFullscreenTransitionStart(
      bool target_fullscreen_state) override {}
  void OnWindowFullscreenTransitionComplete(bool is_fullscreen) override {}
  void OnWindowMiniaturizedChanged(bool miniaturized) override {}
  void OnWindowZoomedChanged(bool zoomed) override {}
  void OnWindowDisplayChanged(const display::Display& display) override {}
  void OnWindowWillClose() override {}
  void OnWindowHasClosed() override {}
  void OnWindowKeyStatusChanged(bool is_key,
                                bool is_content_first_responder,
                                bool full_keyboard_access_enabled) override {}
  void OnWindowStateRestorationDataChanged(
      const std::vector<uint8_t>& data) override {}
  void OnVisibleOnAllWorkspacesChanged(bool visible) override {}
  void OnSheetModalShown() override {}
  void OnSheetModalClosed() override {}
  void OnImmersiveFullscreenToolbarRevealChanged(bool is_revealed) override {}
  void OnImmersiveFullscreenMenuBarRevealChanged(
      double reveal_amount) override {}
  void OnAutohidingMenuBarHeightChanged(int menu_bar_height) override {}
  void DoDialogButtonAction(ui::mojom::DialogButton button) override {}
  void OnFocusWindowToolbar() override {}
  void SetRemoteAccessibilityTokens(
      const std::vector<uint8_t>& window_token,
      const std::vector<uint8_t>& view_token) override {}
  void GetSheetOffsetY(GetSheetOffsetYCallback callback) override {
    std::move(callback).Run(0);
  }
  void DispatchKeyEventRemote(
      std::unique_ptr<ui::Event> event,
      DispatchKeyEventRemoteCallback callback) override {
    std::move(callback).Run(false);
  }
  void DispatchKeyEventToMenuControllerRemote(
      std::unique_ptr<ui::Event> event,
      DispatchKeyEventToMenuControllerRemoteCallback callback) override {
    std::move(callback).Run(false, false);
  }
  void DispatchMonitorEvent(std::unique_ptr<ui::Event> event,
                            bool target_is_this_window,
                            DispatchMonitorEventCallback callback) override {
    std::move(callback).Run(false);
  }
  void GetHasMenuController(GetHasMenuControllerCallback callback) override {
    std::move(callback).Run(false);
  }
  void GetHitTestResult(const gfx::Point& location_in_content,
                        GetHitTestResultCallback callback) override {
    std::move(callback).Run(remote_cocoa::mojom::HitTestResult::kOther);
  }
  void GetTooltipTextAt(const gfx::Point& location_in_content,
                        GetTooltipTextAtCallback callback) override {
    std::move(callback).Run(std::u16string());
  }
  void GetIsFocusedViewTextual(
      GetIsFocusedViewTextualCallback callback) override {
    std::move(callback).Run(false);
  }
  void GetWidgetIsModal(GetWidgetIsModalCallback callback) override {
    std::move(callback).Run(false);
  }
  void GetDialogButtonInfo(ui::mojom::DialogButton button,
                           GetDialogButtonInfoCallback callback) override {
    std::move(callback).Run(false, std::u16string(), false, false);
  }
  void GetDoDialogButtonsExist(
      GetDoDialogButtonsExistCallback callback) override {
    std::move(callback).Run(false);
  }
  void GetShouldShowWindowTitle(
      GetShouldShowWindowTitleCallback callback) override {
    std::move(callback).Run(false);
  }
  void GetCanWindowBecomeKey(GetCanWindowBecomeKeyCallback callback) override {
    std::move(callback).Run(false);
  }
  void GetAlwaysRenderWindowAsKey(
      GetAlwaysRenderWindowAsKeyCallback callback) override {
    std::move(callback).Run(false);
  }
  void OnWindowCloseRequested(
      OnWindowCloseRequestedCallback callback) override {
    std::move(callback).Run(false);
  }
  void GetWindowFrameTitlebarHeight(
      GetWindowFrameTitlebarHeightCallback callback) override {
    std::move(callback).Run(false, 0.0f);
  }
  void GetRootViewAccessibilityToken(
      GetRootViewAccessibilityTokenCallback callback) override {
    std::move(callback).Run(base::kNullProcessId, std::vector<uint8_t>());
  }
  void ValidateUserInterfaceItem(
      int32_t command,
      ValidateUserInterfaceItemCallback callback) override {
    std::move(callback).Run(
        remote_cocoa::mojom::ValidateUserInterfaceItemResult::New());
  }
  void WillExecuteCommand(int32_t command,
                          WindowOpenDisposition window_open_disposition,
                          bool is_before_first_responder,
                          ExecuteCommandCallback callback) override {
    std::move(callback).Run(false);
  }
  void ExecuteCommand(int32_t command,
                      WindowOpenDisposition window_open_disposition,
                      bool is_before_first_responder,
                      ExecuteCommandCallback callback) override {
    std::move(callback).Run(false);
  }
  void HandleAccelerator(const ui::Accelerator& accelerator,
                         bool require_priority_handler,
                         HandleAcceleratorCallback callback) override {
    std::move(callback).Run(false);
  }
};

class DummyNativeWidgetNSWindowHostHelper
    : public remote_cocoa::NativeWidgetNSWindowHostHelper {
 public:
  DummyNativeWidgetNSWindowHostHelper() = default;
  ~DummyNativeWidgetNSWindowHostHelper() override = default;

 private:
  id GetNativeViewAccessible() override { return nil; }
  void DispatchKeyEvent(ui::KeyEvent* event) override {}
  bool DispatchKeyEventToMenuController(ui::KeyEvent* event) override {
    return false;
  }
  void GetWordAt(const gfx::Point& location_in_content,
                 bool* found_word,
                 gfx::DecoratedText* decorated_word,
                 gfx::Point* baseline_point) override {
    *found_word = false;
  }
  remote_cocoa::DragDropClient* GetDragDropClient() override { return nullptr; }
  ui::TextInputClient* GetTextInputClient() override { return nullptr; }
};

}  // namespace

// Tests that AppKit frame adjustments (such as clamping window position to
// screen boundaries) are suppressed when a window move loop is active, allowing
// windows to be dragged smoothly across display boundaries without coordinate
// jumping.
TEST(NativeWidgetMacNSWindowTest,
     SuppressAppKitFrameAdjustmentsDuringMoveLoop) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      remote_cocoa::features::kSuppressAppKitFrameAdjustmentsDuringMoveLoop);

  display::ScopedNativeScreen screen;
  @autoreleasepool {
    NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];

    DummyNativeWidgetNSWindowHost dummy_host;
    DummyNativeWidgetNSWindowHostHelper dummy_helper;
    auto bridge = std::make_unique<remote_cocoa::NativeWidgetNSWindowBridge>(
        1, &dummy_host, &dummy_helper, nullptr);
    bridge->SetWindow(window);
    bridge->SetWindowMoveLoopForTesting(
        std::make_unique<remote_cocoa::CocoaWindowMoveLoop>(bridge.get(),
                                                            NSZeroPoint));

    // Verify that constraining the frame rect returns the requested offscreen
    // frame unchanged without AppKit clamping while the move loop is active.
    NSScreen* main_screen = [NSScreen mainScreen];
    NSRect offscreen_rect = NSMakeRect(-5000, -5000, 400, 400);
    NSRect constrained = [window constrainFrameRect:offscreen_rect
                                           toScreen:main_screen];
    EXPECT_TRUE(NSEqualRects(constrained, offscreen_rect));

    // Save initial frame and verify that AppKit internal frame adjustment
    // callbacks do not modify the frame while the move loop is active.
    NSRect initial_frame = [window frame];

    [window _setFrame:NSMakeRect(200, 200, 400, 400)
        fromAdjustmentToScreen:main_screen
                anchorIfNeeded:nullptr
                       animate:NO];
    EXPECT_TRUE(NSEqualRects([window frame], initial_frame));

    [window _setFrameAfterMove:NSMakeRect(300, 300, 400, 400)];
    EXPECT_TRUE(NSEqualRects([window frame], initial_frame));

    bridge->EndMoveLoop();
    [window close];
  }
}

// Tests that when the feature flag is disabled, default AppKit frame
// constraining is performed even when a move loop is active.
TEST(NativeWidgetMacNSWindowTest,
     SuppressAppKitFrameAdjustmentsDuringMoveLoop_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      remote_cocoa::features::kSuppressAppKitFrameAdjustmentsDuringMoveLoop);

  display::ScopedNativeScreen screen;
  @autoreleasepool {
    NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];

    DummyNativeWidgetNSWindowHost dummy_host;
    DummyNativeWidgetNSWindowHostHelper dummy_helper;
    auto bridge = std::make_unique<remote_cocoa::NativeWidgetNSWindowBridge>(
        2, &dummy_host, &dummy_helper, nullptr);
    bridge->SetWindow(window);

    NSRect new_frame = NSMakeRect(200, 200, 400, 400);
    [window _setFrameAfterMove:new_frame];
    // When feature is disabled, _setFrameAfterMove forwards to super and
    // updates frame.
    EXPECT_TRUE(NSEqualRects([window frame], new_frame));

    [window close];
  }
}

// Tests that AppKit frame adjustments are suppressed specifically when dragging
// across display boundaries, ensuring position is not clamped or locked.
TEST(NativeWidgetMacNSWindowTest,
     SuppressAppKitFrameAdjustmentsAcrossDisplayBoundaries) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      remote_cocoa::features::kSuppressAppKitFrameAdjustmentsDuringMoveLoop);

  display::ScopedNativeScreen screen;
  @autoreleasepool {
    NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];

    DummyNativeWidgetNSWindowHost dummy_host;
    DummyNativeWidgetNSWindowHostHelper dummy_helper;
    auto bridge = std::make_unique<remote_cocoa::NativeWidgetNSWindowBridge>(
        3, &dummy_host, &dummy_helper, nullptr);
    bridge->SetWindow(window);

    // Set up an active move loop.
    bridge->SetWindowMoveLoopForTesting(
        std::make_unique<remote_cocoa::CocoaWindowMoveLoop>(bridge.get(),
                                                            NSZeroPoint));

    NSScreen* main_screen = [NSScreen mainScreen];

    // Simulate window dragging across display boundary line (e.g. x = 1728).
    NSRect boundary_frame = NSMakeRect(1728, 200, 800, 600);
    [window setFrame:boundary_frame display:NO];

    // Verify constrainFrameRect returns the unconstrained boundary frame.
    NSRect constrained = [window constrainFrameRect:boundary_frame
                                           toScreen:main_screen];
    EXPECT_TRUE(NSEqualRects(constrained, boundary_frame));

    // Simulate stale AppKit frame adjustment events firing during drag across
    // boundary.
    NSRect stale_appkit_frame = NSMakeRect(1600, 200, 800, 600);

    [window _setFrame:stale_appkit_frame
        fromAdjustmentToScreen:main_screen
                anchorIfNeeded:nullptr
                       animate:NO];
    EXPECT_TRUE(NSEqualRects([window frame], boundary_frame));

    [window _setFrameAfterMove:stale_appkit_frame];
    EXPECT_TRUE(NSEqualRects([window frame], boundary_frame));

    // End the move loop and verify normal frame updates resume.
    bridge->EndMoveLoop();

    [window _setFrameAfterMove:stale_appkit_frame];
    EXPECT_TRUE(NSEqualRects([window frame], stale_appkit_frame));

    // Verify _setFrame:fromAdjustmentToScreen:anchorIfNeeded:animate: can be
    // invoked safely outside the move loop with a valid or null anchor.
    CGPoint anchor_point = CGPointMake(1600, 200);
    [window _setFrame:boundary_frame
        fromAdjustmentToScreen:main_screen
                anchorIfNeeded:&anchor_point
                       animate:NO];

    EXPECT_EQ(
        [window respondsToSelector:@selector(_setFrame:fromAdjustmentToScreen:
                                             anchorIfNeeded:animate:)],
        [NSWindow
            instancesRespondToSelector:
                @selector(
                    _setFrame:fromAdjustmentToScreen:anchorIfNeeded:animate:)]);
    EXPECT_EQ(
        [window respondsToSelector:@selector(_setFrameAfterMove:)],
        [NSWindow instancesRespondToSelector:@selector(_setFrameAfterMove:)]);

    [window close];
  }
}

// Tests that internal AppKit screen layout adjustments and _setFrameAfterMove:
// do not cause infinite recursion or stack overflow when delegated to super.
TEST(NativeWidgetMacNSWindowTest, AppKitScreenAdjustmentReentrancyAndSafety) {
  display::ScopedNativeScreen screen;
  @autoreleasepool {
    NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];

    DummyNativeWidgetNSWindowHost dummy_host;
    DummyNativeWidgetNSWindowHostHelper dummy_helper;
    auto bridge = std::make_unique<remote_cocoa::NativeWidgetNSWindowBridge>(
        4, &dummy_host, &dummy_helper, nullptr);
    bridge->SetWindow(window);

    NSScreen* main_screen = [NSScreen mainScreen];
    NSRect test_frame = NSMakeRect(100, 100, 500, 400);

    // 1. Calling _setFrame:fromAdjustmentToScreen:anchorIfNeeded:animate: with
    // a valid anchor pointer outside of a move loop must not crash or recurse.
    CGPoint anchor = CGPointMake(100, 100);
    [window _setFrame:test_frame
        fromAdjustmentToScreen:main_screen
                anchorIfNeeded:&anchor
                       animate:NO];

    // 2. Calling _setFrame:fromAdjustmentToScreen:anchorIfNeeded:animate: with
    // a null anchor pointer must not crash or recurse.
    [window _setFrame:test_frame
        fromAdjustmentToScreen:main_screen
                anchorIfNeeded:nullptr
                       animate:NO];

    // 3. Calling _setFrameAfterMove: (which AppKit's implementation routes to
    // _setFrame:fromAdjustmentToScreen:anchor:animate: ->
    // _setFrame:fromAdjustmentToScreen:anchorIfNeeded:animate:) must complete
    // cleanly without infinite recursion.
    [window _setFrameAfterMove:test_frame];

    [window close];
  }
}

// Tests that setting window bounds programmatically updates the
// CocoaWindowMoveLoop base frame during an active move loop.
TEST(NativeWidgetMacNSWindowTest, SetBoundsUpdatesMoveLoopBaseFrame) {
  display::ScopedNativeScreen screen;
  @autoreleasepool {
    NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];

    DummyNativeWidgetNSWindowHost dummy_host;
    DummyNativeWidgetNSWindowHostHelper dummy_helper;
    auto bridge = std::make_unique<remote_cocoa::NativeWidgetNSWindowBridge>(
        4, &dummy_host, &dummy_helper, nullptr);
    bridge->SetWindow(window);

    auto move_loop = std::make_unique<remote_cocoa::CocoaWindowMoveLoop>(
        bridge.get(), NSMakePoint(100, 100));
    bridge->SetWindowMoveLoopForTesting(std::move(move_loop));

    gfx::Rect new_bounds(500, 300, 600, 400);
    bridge->SetBounds(new_bounds, gfx::Size(100, 100), std::nullopt);

    EXPECT_EQ(gfx::ScreenRectFromNSRect([window frame]), new_bounds);
    EXPECT_TRUE(NSEqualRects(
        bridge->window_move_loop()->base_frame_for_testing(), [window frame]));
    EXPECT_TRUE(
        NSEqualRects(bridge->window_move_loop()->last_set_frame_for_testing(),
                     [window frame]));

    bridge->EndMoveLoop();
    [window close];
  }
}
