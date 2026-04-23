// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Browser test for https://crbug.com/325931972.
//
// Reproduces the "zombie window" DumpWithoutCrashing that occurs when macOS
// system frameworks (e.g., AuthenticationServicesCore presenting passkey/
// WebAuthn dialogs) internally call orderWindow: on the invisible browser-side
// proxy window.
// We can't trigger the system passkey dialog in a test, but we CAN directly
// call orderFront:/orderWindow: on the proxy window — which is exactly what
// AppKit does internally. This test verifies our orderWindow: guard works.

#import <Cocoa/Cocoa.h>

#include "chrome/test/base/in_process_browser_test.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "content/public/test/browser_test.h"
#include "ui/base/cocoa/window_size_constants.h"

class NativeWidgetMacZombieWindowBrowserTest : public InProcessBrowserTest {};

// In production, AppKit calls orderWindow:relativeTo: on the invisible
// browser-side proxy window (created by CreateRemoteNSWindow with
// enforceNeverMadeVisible). This happens when system frameworks present
// modal windows — e.g., AuthenticationServicesCore for passkey dialogs,
// or AppKit's own sheet ordering. The orderWindow: call makes the proxy
// visible, which fires KVO and triggers DumpWithoutCrashing.
//
// This test directly calls orderFront: on the proxy — the same AppKit
// method that internally dispatches to orderWindow:NSWindowAbove. Without
// our fix, the window becomes visible and the EXPECT_FALSE fails.
IN_PROC_BROWSER_TEST_F(NativeWidgetMacZombieWindowBrowserTest,
                       OrderFrontDoesNotShowProxyWindow) {
  // Create a proxy window the same way CreateRemoteNSWindow does:
  // borderless, alpha 0, enforceNeverMadeVisible.
  NativeWidgetMacNSWindow* proxy_window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  [proxy_window setAlphaValue:0.0];
  [proxy_window enforceNeverMadeVisible];
  ASSERT_FALSE([proxy_window isVisible]);

  // This is exactly what AppKit does internally when presenting modal
  // windows. orderFront: dispatches to orderWindow:NSWindowAbove relativeTo:0.
  // Without the fix in orderWindow:, this makes the proxy visible and
  // triggers DumpWithoutCrashing via the KVO observer.
  [proxy_window orderFront:nil];
  EXPECT_FALSE([proxy_window isVisible]);

  // Also test orderWindow: directly (the method our fix guards).
  [proxy_window orderWindow:NSWindowAbove relativeTo:0];
  EXPECT_FALSE([proxy_window isVisible]);

  [proxy_window orderWindow:NSWindowBelow relativeTo:0];
  EXPECT_FALSE([proxy_window isVisible]);

  // Ordering out should still be allowed.
  [proxy_window orderWindow:NSWindowOut relativeTo:0];
  EXPECT_FALSE([proxy_window isVisible]);

  [proxy_window close];
}
