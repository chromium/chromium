// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"

#include <AppKit/AppKit.h>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/test/mock_chrome_application_mac.h"
#include "base/test/scoped_feature_list.h"
#include "components/remote_cocoa/app_shim/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cocoa/window_size_constants.h"

namespace {

NSWindow* g_fake_key_window = nil;

}  // namespace

// A window that opts in to its ancestors updating the cursor while it is key.
@interface TestOptInCursorNSWindow : NativeWidgetMacNSWindow
@end

@implementation TestOptInCursorNSWindow
- (BOOL)ancestorsUpdateCursorWhileKey {
  return YES;
}
@end

// Donates -[NSApplication keyWindow], since windows can't become key in a unit
// test.
@interface FakeKeyWindowNSApplicationDonor : NSObject
@end

@implementation FakeKeyWindowNSApplicationDonor
- (NSWindow*)keyWindow {
  return g_fake_key_window;
}
@end

namespace {

NativeWidgetMacNSWindow* CreateWindow() {
  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  window.releasedWhenClosed = NO;
  return window;
}

TestOptInCursorNSWindow* CreateOptInWindow() {
  TestOptInCursorNSWindow* window = [[TestOptInCursorNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  window.releasedWhenClosed = NO;
  return window;
}

// KeyWindowDelegatesCursorTo() reads [NSApp keyWindow], so the swizzled
// -keyWindow only runs once an application object exists.
void EnsureNSApp() {
  if (!NSApp) {
    mock_cr_app::RegisterMockCrApp();
  }
}

TEST(NativeWidgetMacNSWindowTest, ImmersiveRevealLockDoesNotRequireKey) {
  NativeWidgetMacNSWindow* window = CreateWindow();
  EXPECT_FALSE([window immersiveRevealLockRequiresKey]);
  [window close];
}

TEST(NativeWidgetMacNSWindowTest, AncestorsUpdateCursorDefaultsToNo) {
  NativeWidgetMacNSWindow* window = CreateWindow();
  EXPECT_FALSE([window ancestorsUpdateCursorWhileKey]);
  [window close];
}

// The key window delegates cursor handling to its ancestors only if it opted
// in and the feature is enabled.
TEST(NativeWidgetMacNSWindowTest, KeyWindowDelegatesCursorTo) {
  EnsureNSApp();

  g_fake_key_window = nil;
  base::apple::ScopedObjCClassSwizzler key_window_swizzler(
      [NSApplication class], [FakeKeyWindowNSApplicationDonor class],
      @selector(keyWindow));

  NativeWidgetMacNSWindow* grandparent = CreateWindow();
  NativeWidgetMacNSWindow* parent = CreateWindow();
  NativeWidgetMacNSWindow* unrelated = CreateWindow();
  NativeWidgetMacNSWindow* plain_child = CreateWindow();
  TestOptInCursorNSWindow* opt_in_child = CreateOptInWindow();

  [grandparent addChildWindow:parent ordered:NSWindowAbove];
  [parent addChildWindow:plain_child ordered:NSWindowAbove];
  [parent addChildWindow:opt_in_child ordered:NSWindowAbove];

  ASSERT_EQ(grandparent, parent.parentWindow);
  ASSERT_EQ(parent, plain_child.parentWindow);
  ASSERT_EQ(parent, opt_in_child.parentWindow);

  // A key child that opted in delegates to every ancestor, but not to itself
  // or to unrelated windows.
  g_fake_key_window = opt_in_child;
  EXPECT_TRUE(remote_cocoa::KeyWindowDelegatesCursorTo(parent));
  EXPECT_TRUE(remote_cocoa::KeyWindowDelegatesCursorTo(grandparent));
  EXPECT_FALSE(remote_cocoa::KeyWindowDelegatesCursorTo(opt_in_child));
  EXPECT_FALSE(remote_cocoa::KeyWindowDelegatesCursorTo(unrelated));
  EXPECT_FALSE(remote_cocoa::KeyWindowDelegatesCursorTo(nil));

  // A key child that did not opt in does not delegate.
  g_fake_key_window = plain_child;
  EXPECT_FALSE(remote_cocoa::KeyWindowDelegatesCursorTo(parent));

  // Nothing delegates while the feature is disabled.
  g_fake_key_window = opt_in_child;
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        remote_cocoa::features::kMacCursorUpdatesUnderKeyChildWindow);
    EXPECT_FALSE(remote_cocoa::KeyWindowDelegatesCursorTo(parent));
  }

  g_fake_key_window = nil;
  [opt_in_child close];
  [plain_child close];
  [unrelated close];
  [parent close];
  [grandparent close];
}

// The key window's ancestors are found through -parentWindow alone, so a window
// in between that isn't a NativeWidgetMacNSWindow doesn't end the search. In
// immersive fullscreen the omnibox popup is a child of the overlay window,
// which is a child of AppKit's NSToolbarFullScreenWindow, which is a child of
// the browser window.
TEST(NativeWidgetMacNSWindowTest,
     KeyWindowDelegatesCursorThroughNonWidgetWindow) {
  EnsureNSApp();

  g_fake_key_window = nil;
  base::apple::ScopedObjCClassSwizzler key_window_swizzler(
      [NSApplication class], [FakeKeyWindowNSApplicationDonor class],
      @selector(keyWindow));

  NativeWidgetMacNSWindow* browser = CreateWindow();
  // Stands in for AppKit's NSToolbarFullScreenWindow.
  NSWindow* toolbar_window =
      [[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  toolbar_window.releasedWhenClosed = NO;
  NativeWidgetMacNSWindow* overlay = CreateWindow();
  TestOptInCursorNSWindow* popup = CreateOptInWindow();

  [browser addChildWindow:toolbar_window ordered:NSWindowAbove];
  [toolbar_window addChildWindow:overlay ordered:NSWindowAbove];
  [overlay addChildWindow:popup ordered:NSWindowAbove];

  ASSERT_EQ(browser, toolbar_window.parentWindow);
  ASSERT_EQ(toolbar_window, overlay.parentWindow);
  ASSERT_EQ(overlay, popup.parentWindow);

  g_fake_key_window = popup;
  EXPECT_TRUE(remote_cocoa::KeyWindowDelegatesCursorTo(overlay));
  EXPECT_TRUE(remote_cocoa::KeyWindowDelegatesCursorTo(browser));

  g_fake_key_window = nil;
  [popup close];
  [overlay close];
  [toolbar_window close];
  [browser close];
}

// Becoming or resigning key is reported to the parent's
// `childWindowKeyChangedHandler`, if it has one.
TEST(NativeWidgetMacNSWindowTest, ChildWindowKeyChangedHandler) {
  NativeWidgetMacNSWindow* parent = CreateWindow();
  NativeWidgetMacNSWindow* child = CreateWindow();
  [parent addChildWindow:child ordered:NSWindowAbove];

  NSMutableArray<NSWindow*>* reported = [NSMutableArray array];
  parent.childWindowKeyChangedHandler = ^(NSWindow* window) {
    [reported addObject:window];
  };
  [child becomeKeyWindow];
  [child resignKeyWindow];
  EXPECT_EQ(reported.count, 2u);
  EXPECT_EQ(reported.firstObject, child);
  EXPECT_EQ(reported.lastObject, child);

  // Nothing is reported without a handler or once the window is detached.
  parent.childWindowKeyChangedHandler = nil;
  [child becomeKeyWindow];
  [child resignKeyWindow];
  parent.childWindowKeyChangedHandler = ^(NSWindow* window) {
    [reported addObject:window];
  };
  [parent removeChildWindow:child];
  [child becomeKeyWindow];
  [child resignKeyWindow];
  EXPECT_EQ(reported.count, 2u);

  [child close];
  [parent close];
}

}  // namespace
