// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/native_app_window.h"

#import <Cocoa/Cocoa.h>
#include <memory>

#import "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#import "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"
#include "chrome/browser/apps/app_shim/test/app_shim_listener_test_api_mac.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/constants.h"
#include "skia/ext/skia_utils_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "ui/base/test/nswindow_fullscreen_notification_waiter.h"
#import "ui/base/test/scoped_fake_nswindow_focus.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#import "ui/base/test/windowed_nsnotification_observer.h"

using extensions::AppWindow;
using extensions::PlatformAppBrowserTest;

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace {

bool IsNSWindowFloating(NSWindow* window) {
  return [window level] != NSNormalWindowLevel;
}

class NativeAppWindowCocoaBrowserTest : public PlatformAppBrowserTest {
 protected:
  NativeAppWindowCocoaBrowserTest() {}

  void SetUpAppWithWindows(int num_windows) {
    app_ = InstallExtension(
        test_data_dir_.AppendASCII("platform_apps").AppendASCII("minimal"), 1);
    EXPECT_TRUE(app_);

    for (int i = 0; i < num_windows; ++i) {
      content::WindowedNotificationObserver app_loaded_observer(
          content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
          content::NotificationService::AllSources());
      apps::LaunchService::Get(profile())->OpenApplication(
          apps::AppLaunchParams(
              app_->id(), apps::mojom::LaunchContainer::kLaunchContainerNone,
              WindowOpenDisposition::NEW_WINDOW,
              apps::mojom::AppLaunchSource::kSourceTest));
      app_loaded_observer.Wait();
    }
  }

  const extensions::Extension* app_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeAppWindowCocoaBrowserTest);
};

}  // namespace

// Test interaction of Hide/Show() with Hide/Show(). Historically this had
// tricky behavior for apps, but now behaves as one would expect.
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, HideShow) {
  SetUpAppWithWindows(2);
  extensions::AppWindowRegistry::AppWindowList windows =
      extensions::AppWindowRegistry::Get(profile())->app_windows();

  AppWindow* app_window = windows.front();
  extensions::NativeAppWindow* native_window = app_window->GetBaseWindow();
  NSWindow* ns_window = native_window->GetNativeWindow().GetNativeNSWindow();

  AppWindow* other_app_window = windows.back();
  extensions::NativeAppWindow* other_native_window =
      other_app_window->GetBaseWindow();
  NSWindow* other_ns_window =
      other_native_window->GetNativeWindow().GetNativeNSWindow();

  // Normal Hide/Show.
  app_window->Hide();
  EXPECT_FALSE([ns_window isVisible]);
  app_window->Show(AppWindow::SHOW_ACTIVE);
  EXPECT_TRUE([ns_window isVisible]);

  // Normal Hide/Show.
  native_window->Hide();
  EXPECT_FALSE([ns_window isVisible]);
  native_window->Show();
  EXPECT_TRUE([ns_window isVisible]);

  // Hide, Hide, Show shows.
  native_window->Hide();
  app_window->Hide();
  native_window->Show();
  EXPECT_TRUE([ns_window isVisible]);

  // Hide, Show still shows.
  native_window->Hide();
  native_window->Show();
  EXPECT_TRUE([ns_window isVisible]);

  // Return to shown state.
  app_window->Show(AppWindow::SHOW_ACTIVE);
  EXPECT_TRUE([ns_window isVisible]);

  // Hide the other window.
  EXPECT_TRUE([other_ns_window isVisible]);
  other_native_window->Hide();
  EXPECT_FALSE([other_ns_window isVisible]);

  // Hide, Show shows just one window since there's no shim.
  native_window->Hide();
  EXPECT_FALSE([ns_window isVisible]);
  app_window->Show(AppWindow::SHOW_ACTIVE);
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_FALSE([other_ns_window isVisible]);

  // Hide the other window.
  other_app_window->Hide();
  EXPECT_FALSE([other_ns_window isVisible]);

  // Hide, Show does not show the other window.
  native_window->Hide();
  EXPECT_FALSE([ns_window isVisible]);
  native_window->Show();
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_FALSE([other_ns_window isVisible]);
}

// Test Hide/Show and Hide/Show() behavior when shims are enabled.
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, HideShowWithShim) {
  test::AppShimListenerTestApi test_api(
      g_browser_process->platform_part()->app_shim_listener());
  SetUpAppWithWindows(1);
  extensions::AppWindowRegistry::AppWindowList windows =
      extensions::AppWindowRegistry::Get(profile())->app_windows();

  extensions::AppWindow* app_window = windows.front();
  extensions::NativeAppWindow* native_window = app_window->GetBaseWindow();
  NSWindow* ns_window = native_window->GetNativeWindow().GetNativeNSWindow();

  // Hide.
  native_window->Hide();
  EXPECT_FALSE([ns_window isVisible]);

  // Show notifies the shim to unhide.
  app_window->Show(extensions::AppWindow::SHOW_ACTIVE);
  EXPECT_TRUE([ns_window isVisible]);

  // Hide
  native_window->Hide();
  EXPECT_FALSE([ns_window isVisible]);

  // Activate does the same.
  native_window->Activate();
  EXPECT_TRUE([ns_window isVisible]);
}

// Test that NativeAppWindow and AppWindow fullscreen state is updated when
// the window is fullscreened natively.
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, Fullscreen) {
  extensions::AppWindow* app_window =
      CreateTestAppWindow("{\"alwaysOnTop\": true }");
  extensions::NativeAppWindow* window = app_window->GetBaseWindow();
  NSWindow* ns_window = app_window->GetNativeWindow().GetNativeNSWindow();
  base::scoped_nsobject<NSWindowFullscreenNotificationWaiter> waiter(
      [[NSWindowFullscreenNotificationWaiter alloc] initWithWindow:ns_window]);

  EXPECT_EQ(AppWindow::FULLSCREEN_TYPE_NONE,
            app_window->fullscreen_types_for_test());
  EXPECT_FALSE(window->IsFullscreen());
  EXPECT_FALSE([ns_window styleMask] & NSFullScreenWindowMask);
  EXPECT_TRUE(IsNSWindowFloating(ns_window));

  [ns_window toggleFullScreen:nil];
  [waiter waitForEnterCount:1 exitCount:0];
  EXPECT_TRUE(app_window->fullscreen_types_for_test() &
              AppWindow::FULLSCREEN_TYPE_OS);
  EXPECT_TRUE(window->IsFullscreen());
  EXPECT_TRUE([ns_window styleMask] & NSFullScreenWindowMask);
  EXPECT_FALSE(IsNSWindowFloating(ns_window));

  app_window->Restore();
  EXPECT_FALSE(window->IsFullscreenOrPending());
  [waiter waitForEnterCount:1 exitCount:1];
  EXPECT_EQ(AppWindow::FULLSCREEN_TYPE_NONE,
            app_window->fullscreen_types_for_test());
  EXPECT_FALSE(window->IsFullscreen());
  EXPECT_FALSE([ns_window styleMask] & NSFullScreenWindowMask);
  EXPECT_TRUE(IsNSWindowFloating(ns_window));

  app_window->Fullscreen();
  EXPECT_TRUE(window->IsFullscreenOrPending());
  [waiter waitForEnterCount:2 exitCount:1];
  EXPECT_TRUE(app_window->fullscreen_types_for_test() &
              AppWindow::FULLSCREEN_TYPE_WINDOW_API);
  EXPECT_TRUE(window->IsFullscreen());
  EXPECT_TRUE([ns_window styleMask] & NSFullScreenWindowMask);
  EXPECT_FALSE(IsNSWindowFloating(ns_window));

  [ns_window toggleFullScreen:nil];
  [waiter waitForEnterCount:2 exitCount:2];
  EXPECT_EQ(AppWindow::FULLSCREEN_TYPE_NONE,
            app_window->fullscreen_types_for_test());
  EXPECT_FALSE(window->IsFullscreen());
  EXPECT_FALSE([ns_window styleMask] & NSFullScreenWindowMask);
  EXPECT_TRUE(IsNSWindowFloating(ns_window));
}

// Test Minimize, Restore combinations with their native equivalents.
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, Minimize) {
  if (base::mac::IsOS10_10())
    return;  // Fails when swarmed. http://crbug.com/660582
  SetUpAppWithWindows(1);
  AppWindow* app_window = GetFirstAppWindow();
  extensions::NativeAppWindow* window = app_window->GetBaseWindow();
  NSWindow* ns_window = app_window->GetNativeWindow().GetNativeNSWindow();

  NSRect initial_frame = [ns_window frame];

  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);

  // Native minimize, Restore.
  [ns_window miniaturize:nil];
  EXPECT_NSEQ(initial_frame, [ns_window frame]);
  EXPECT_TRUE(window->IsMinimized());
  EXPECT_TRUE([ns_window isMiniaturized]);

  app_window->Restore();
  EXPECT_NSEQ(initial_frame, [ns_window frame]);
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);

  // Minimize, native restore.
  app_window->Minimize();
  EXPECT_NSEQ(initial_frame, [ns_window frame]);
  EXPECT_TRUE(window->IsMinimized());
  EXPECT_TRUE([ns_window isMiniaturized]);

  [ns_window deminiaturize:nil];
  EXPECT_NSEQ(initial_frame, [ns_window frame]);
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);
}

// Test Maximize, Restore combinations with their native equivalents.
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, Maximize) {
  SetUpAppWithWindows(1);
  AppWindow* app_window = GetFirstAppWindow();
  extensions::NativeAppWindow* window = app_window->GetBaseWindow();
  NSWindow* ns_window = app_window->GetNativeWindow().GetNativeNSWindow();
  base::scoped_nsobject<WindowedNSNotificationObserver> watcher;

  gfx::Rect initial_restored_bounds = window->GetRestoredBounds();
  NSRect initial_frame = [ns_window frame];
  NSRect maximized_frame = [[ns_window screen] visibleFrame];

  EXPECT_FALSE(window->IsMaximized());

  // Native maximize, Restore.
  watcher.reset([[WindowedNSNotificationObserver alloc]
      initForNotification:NSWindowDidResizeNotification
                   object:ns_window]);
  [ns_window zoom:nil];
  [watcher wait];
  EXPECT_EQ(initial_restored_bounds, window->GetRestoredBounds());
  EXPECT_NSEQ(maximized_frame, [ns_window frame]);
  EXPECT_TRUE(window->IsMaximized());

  watcher.reset([[WindowedNSNotificationObserver alloc]
      initForNotification:NSWindowDidResizeNotification
                   object:ns_window]);
  app_window->Restore();
  [watcher wait];
  EXPECT_EQ(initial_restored_bounds, window->GetRestoredBounds());
  EXPECT_NSEQ(initial_frame, [ns_window frame]);
  EXPECT_FALSE(window->IsMaximized());

  // Maximize, native restore.
  watcher.reset([[WindowedNSNotificationObserver alloc]
      initForNotification:NSWindowDidResizeNotification
                   object:ns_window]);
  app_window->Maximize();
  [watcher wait];
  EXPECT_EQ(initial_restored_bounds, window->GetRestoredBounds());
  EXPECT_NSEQ(maximized_frame, [ns_window frame]);
  EXPECT_TRUE(window->IsMaximized());

  watcher.reset([[WindowedNSNotificationObserver alloc]
      initForNotification:NSWindowDidResizeNotification
                   object:ns_window]);
  [ns_window zoom:nil];
  [watcher wait];
  EXPECT_EQ(initial_restored_bounds, window->GetRestoredBounds());
  EXPECT_NSEQ(initial_frame, [ns_window frame]);
  EXPECT_FALSE(window->IsMaximized());
}

// Test Maximize when the window has a maximum size. The maximum size means that
// the window is not user-maximizable. However, calling Maximize() via the
// javascript API should still maximize and since the zoom button is removed,
// the codepath changes.
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, MaximizeConstrained) {
  AppWindow* app_window = CreateTestAppWindow(
      "{\"outerBounds\": {\"maxWidth\":200, \"maxHeight\":300}}");
  extensions::NativeAppWindow* window = app_window->GetBaseWindow();
  NSWindow* ns_window = app_window->GetNativeWindow().GetNativeNSWindow();
  base::scoped_nsobject<WindowedNSNotificationObserver> watcher;

  gfx::Rect initial_restored_bounds = window->GetRestoredBounds();
  NSRect initial_frame = [ns_window frame];
  NSRect maximized_frame = [[ns_window screen] visibleFrame];

  EXPECT_FALSE(window->IsMaximized());

  // Maximize, Restore.
  watcher.reset([[WindowedNSNotificationObserver alloc]
      initForNotification:NSWindowDidResizeNotification
                   object:ns_window]);
  app_window->Maximize();
  [watcher wait];
  EXPECT_EQ(initial_restored_bounds, window->GetRestoredBounds());
  EXPECT_NSEQ(maximized_frame, [ns_window frame]);
  EXPECT_TRUE(window->IsMaximized());

  watcher.reset([[WindowedNSNotificationObserver alloc]
      initForNotification:NSWindowDidResizeNotification
                   object:ns_window]);
  app_window->Restore();
  [watcher wait];
  EXPECT_EQ(initial_restored_bounds, window->GetRestoredBounds());
  EXPECT_NSEQ(initial_frame, [ns_window frame]);
  EXPECT_FALSE(window->IsMaximized());
}

// Test Minimize, Maximize, Restore combinations with their native equivalents.
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, MinimizeMaximize) {
  if (base::mac::IsOS10_10())
    return;  // Fails when swarmed. http://crbug.com/660582
  SetUpAppWithWindows(1);
  AppWindow* app_window = GetFirstAppWindow();
  extensions::NativeAppWindow* window = app_window->GetBaseWindow();
  NSWindow* ns_window = app_window->GetNativeWindow().GetNativeNSWindow();
  base::scoped_nsobject<WindowedNSNotificationObserver> watcher;

  NSRect initial_frame = [ns_window frame];
  NSRect maximized_frame = [[ns_window screen] visibleFrame];

  EXPECT_FALSE(window->IsMaximized());
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);

  // Maximize, Minimize, Restore.
  watcher.reset([[WindowedNSNotificationObserver alloc]
      initForNotification:NSWindowDidResizeNotification
                   object:ns_window]);
  app_window->Maximize();
  [watcher wait];
  EXPECT_NSEQ(maximized_frame, [ns_window frame]);
  EXPECT_TRUE(window->IsMaximized());

  app_window->Minimize();
  EXPECT_NSEQ(maximized_frame, [ns_window frame]);
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_TRUE(window->IsMinimized());
  EXPECT_TRUE([ns_window isMiniaturized]);

  app_window->Restore();
  EXPECT_NSEQ(initial_frame, [ns_window frame]);
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);

  // Minimize, Maximize.
  app_window->Minimize();
  EXPECT_NSEQ(initial_frame, [ns_window frame]);
  EXPECT_TRUE(window->IsMinimized());
  EXPECT_TRUE([ns_window isMiniaturized]);

  app_window->Maximize();
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_NSEQ(maximized_frame, [ns_window frame]);
  EXPECT_TRUE(window->IsMaximized());
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);
}

// Test Maximize, Fullscreen, Restore combinations.
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, MaximizeFullscreen) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;

  SetUpAppWithWindows(1);
  AppWindow* app_window = GetFirstAppWindow();
  extensions::NativeAppWindow* window = app_window->GetBaseWindow();
  NSWindow* ns_window = app_window->GetNativeWindow().GetNativeNSWindow();
  base::scoped_nsobject<WindowedNSNotificationObserver> watcher;
  base::scoped_nsobject<NSWindowFullscreenNotificationWaiter> waiter(
      [[NSWindowFullscreenNotificationWaiter alloc] initWithWindow:ns_window]);

  NSRect initial_frame = [ns_window frame];
  NSRect maximized_frame = [[ns_window screen] visibleFrame];

  EXPECT_FALSE(window->IsMaximized());
  EXPECT_FALSE(window->IsFullscreen());

  // Maximize, Fullscreen, Restore, Restore.
  watcher.reset([[WindowedNSNotificationObserver alloc]
      initForNotification:NSWindowDidResizeNotification
                   object:ns_window]);
  app_window->Maximize();
  [watcher wait];
  EXPECT_NSEQ(maximized_frame, [ns_window frame]);
  EXPECT_TRUE(window->IsMaximized());

  EXPECT_EQ(0, [waiter enterCount]);
  app_window->Fullscreen();
  [waiter waitForEnterCount:1 exitCount:0];
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_TRUE(window->IsFullscreen());

  app_window->Restore();
  [waiter waitForEnterCount:1 exitCount:1];
  EXPECT_NSEQ(maximized_frame, [ns_window frame]);
  EXPECT_TRUE(window->IsMaximized());
  EXPECT_FALSE(window->IsFullscreen());

  app_window->Restore();
  EXPECT_NSEQ(initial_frame, [ns_window frame]);
  EXPECT_FALSE(window->IsMaximized());

  // Fullscreen, Maximize, Restore.
  app_window->Fullscreen();
  [waiter waitForEnterCount:2 exitCount:1];
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_TRUE(window->IsFullscreen());

  app_window->Maximize();
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_TRUE(window->IsFullscreen());

  app_window->Restore();
  [waiter waitForEnterCount:2 exitCount:2];
  EXPECT_NSEQ(initial_frame, [ns_window frame]);
  EXPECT_FALSE(window->IsMaximized());
  EXPECT_FALSE(window->IsFullscreen());
}

// Test that, in frameless windows, the web contents has the same size as the
// window.
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, Frameless) {
  AppWindow* app_window = CreateTestAppWindow("{\"frame\": \"none\"}");
  NSWindow* ns_window = app_window->GetNativeWindow().GetNativeNSWindow();
  NSView* web_contents =
      app_window->web_contents()->GetNativeView().GetNativeNSView();
  EXPECT_TRUE(NSEqualSizes(NSMakeSize(512, 384), [web_contents frame].size));
  // Move and resize the window.
  NSRect new_frame = NSMakeRect(50, 50, 200, 200);
  [ns_window setFrame:new_frame display:YES];
  EXPECT_TRUE(NSEqualSizes(new_frame.size, [web_contents frame].size));

  // Windows created with NSBorderlessWindowMask by default don't have shadow,
  // but packaged apps should always have one.
  EXPECT_TRUE([ns_window hasShadow]);

  // Since the window has no constraints, it should have all of the following
  // style mask bits.
  NSUInteger style_mask = NSTitledWindowMask | NSClosableWindowMask |
                          NSMiniaturizableWindowMask | NSResizableWindowMask;
  EXPECT_EQ(style_mask, [ns_window styleMask] & style_mask);

  CloseAppWindow(app_window);
}

namespace {

// Test that resize and fullscreen controls are correctly enabled/disabled.
void TestControls(AppWindow* app_window) {
  NSWindow* ns_window = app_window->GetNativeWindow().GetNativeNSWindow();

  // The window is resizable.
  EXPECT_TRUE([ns_window styleMask] & NSResizableWindowMask);

  // Due to this bug: http://crbug.com/362039, which manifests on the Cocoa
  // implementation but not the views one, frameless windows should have
  // fullscreen controls disabled.
  BOOL can_fullscreen =
      ![NSStringFromClass([ns_window class]) isEqualTo:@"AppFramelessNSWindow"];
  // The window can fullscreen and maximize.
  EXPECT_EQ(can_fullscreen, !!([ns_window collectionBehavior] &
                               NSWindowCollectionBehaviorFullScreenPrimary));

  // Set a maximum size.
  app_window->SetContentSizeConstraints(gfx::Size(), gfx::Size(200, 201));
  EXPECT_EQ(200, [ns_window contentMaxSize].width);
  EXPECT_EQ(201, [ns_window contentMaxSize].height);
  NSView* web_contents =
      app_window->web_contents()->GetNativeView().GetNativeNSView();
  EXPECT_EQ(200, [web_contents frame].size.width);
  EXPECT_EQ(201, [web_contents frame].size.height);

  // Still resizable.
  EXPECT_TRUE([ns_window styleMask] & NSResizableWindowMask);

  // Fullscreen and maximize are disabled.
  EXPECT_FALSE([ns_window collectionBehavior] &
               NSWindowCollectionBehaviorFullScreenPrimary);
  EXPECT_FALSE([[ns_window standardWindowButton:NSWindowZoomButton] isEnabled]);

  // Set a minimum size equal to the maximum size.
  app_window->SetContentSizeConstraints(gfx::Size(200, 201),
                                        gfx::Size(200, 201));
  EXPECT_EQ(200, [ns_window contentMinSize].width);
  EXPECT_EQ(201, [ns_window contentMinSize].height);

  // No longer resizable.
  EXPECT_FALSE([ns_window styleMask] & NSResizableWindowMask);

  // If a window is made fullscreen by the API, fullscreen should be enabled so
  // the user can exit fullscreen.
  base::scoped_nsobject<NSWindowFullscreenNotificationWaiter> waiter(
      [[NSWindowFullscreenNotificationWaiter alloc] initWithWindow:ns_window]);
  app_window->SetFullscreen(AppWindow::FULLSCREEN_TYPE_WINDOW_API, true);
  [waiter waitForEnterCount:1 exitCount:0];
  EXPECT_TRUE([ns_window collectionBehavior] &
              NSWindowCollectionBehaviorFullScreenPrimary);
  EXPECT_EQ(NSWidth([[ns_window contentView] frame]),
            NSWidth([ns_window frame]));
  // Once it leaves fullscreen, it is disabled again.
  app_window->SetFullscreen(AppWindow::FULLSCREEN_TYPE_WINDOW_API, false);
  [waiter waitForEnterCount:1 exitCount:1];
  EXPECT_FALSE([ns_window collectionBehavior] &
               NSWindowCollectionBehaviorFullScreenPrimary);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, Controls) {
  TestControls(CreateTestAppWindow("{}"));
}

IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, ControlsFrameless) {
  TestControls(CreateTestAppWindow("{\"frame\": \"none\"}"));
}

namespace {

// Convert a color constant to an NSColor that can be compared with |bitmap|.
NSColor* ColorInBitmapColorSpace(SkColor color, NSBitmapImageRep* bitmap) {
  return [skia::SkColorToSRGBNSColor(color)
      colorUsingColorSpace:[bitmap colorSpace]];
}

// Take a screenshot of the window, including its native frame.
NSBitmapImageRep* ScreenshotNSWindow(NSWindow* window) {
  NSView* frame_view = [[window contentView] superview];
  NSRect bounds = [frame_view bounds];
  NSBitmapImageRep* bitmap =
      [frame_view bitmapImageRepForCachingDisplayInRect:bounds];
  [frame_view cacheDisplayInRect:bounds toBitmapImageRep:bitmap];
  return bitmap;
}

}  // namespace

// Test that the colored frames have the correct color when active and inactive.
IN_PROC_BROWSER_TEST_F(NativeAppWindowCocoaBrowserTest, FrameColor) {
  // The hex values indicate an RGB color. When we get the NSColor later, the
  // components are CGFloats in the range [0, 1].
  extensions::AppWindow* app_window = CreateTestAppWindow(
      "{\"frame\": {\"color\": \"#FF0000\", \"inactiveColor\": \"#0000FF\"}}");
  NSWindow* ns_window = app_window->GetNativeWindow().GetNativeNSWindow();
  // No color correction in the default case.
  [ns_window setColorSpace:[NSColorSpace sRGBColorSpace]];

  int half_width = NSWidth([ns_window frame]) / 2;

  NSBitmapImageRep* bitmap = ScreenshotNSWindow(ns_window);
  // The window is currently inactive so it should be blue (#0000FF).
  NSColor* expected_color = ColorInBitmapColorSpace(0xFF0000FF, bitmap);
  NSColor* color = [bitmap colorAtX:half_width y:5];
  CGFloat expected_components[4], color_components[4];
  [expected_color getComponents:expected_components];
  [color getComponents:color_components];
  EXPECT_NEAR(expected_components[0], color_components[0], 0.01);
  EXPECT_NEAR(expected_components[1], color_components[1], 0.01);
  EXPECT_NEAR(expected_components[2], color_components[2], 0.01);

  ui::test::ScopedFakeNSWindowFocus fake_focus;
  [ns_window makeMainWindow];

  bitmap = ScreenshotNSWindow(ns_window);
  // The window is now active so it should be red (#FF0000).
  expected_color = ColorInBitmapColorSpace(0xFFFF0000, bitmap);
  color = [bitmap colorAtX:half_width y:5];
  [expected_color getComponents:expected_components];
  [color getComponents:color_components];
  EXPECT_NEAR(expected_components[0], color_components[0], 0.01);
  EXPECT_NEAR(expected_components[1], color_components[1], 0.01);
  EXPECT_NEAR(expected_components[2], color_components[2], 0.01);
}
