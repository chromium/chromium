// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/omnibox/omnibox_popup_native_widget_mac_test_util.h"

#import <Cocoa/Cocoa.h>

#import "base/apple/foundation_util.h"
#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/check.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "ui/views/widget/widget.h"

namespace {

// The root NSWindow whose hierarchy reports being off the active Space.
NSWindow* __strong g_inactive_space_root_window = nil;

// Swizzles -isOnActiveSpace while a ScopedSimulateInactiveSpace is alive.
base::apple::ScopedObjCClassSwizzler* g_active_space_swizzler = nullptr;

// Walks up both the NSWindow parents and the bridge parents. In immersive
// fullscreen, the overlay window's NSWindow parent is AppKit's fullscreen
// toolbar window, while its bridge parent is the browser window.
bool BelongsToInactiveSpaceRoot(NSWindow* window) {
  for (NSWindow* current = window; current;) {
    if (current == g_inactive_space_root_window) {
      return true;
    }
    if (current.parentWindow) {
      current = current.parentWindow;
      continue;
    }
    auto* bridge_window =
        base::apple::ObjCCast<NativeWidgetMacNSWindow>(current);
    remote_cocoa::NativeWidgetNSWindowBridge* parent_bridge =
        bridge_window.bridge ? bridge_window.bridge->parent() : nullptr;
    current = parent_bridge ? parent_bridge->ns_window() : nil;
  }
  return false;
}

}  // namespace

// Provides the -isOnActiveSpace implementation swapped into NSWindow.
@interface InactiveSpaceDonorForTesting : NSObject
- (BOOL)isOnActiveSpace;
@end

@implementation InactiveSpaceDonorForTesting

- (BOOL)isOnActiveSpace {
  NSWindow* window = static_cast<NSWindow*>(self);
  if (BelongsToInactiveSpaceRoot(window)) {
    return NO;
  }
  return g_active_space_swizzler->InvokeOriginal<BOOL>(self, _cmd);
}

@end

struct ScopedSimulateInactiveSpace::ObjCStorage {
  ObjCStorage()
      : swizzler([NativeWidgetMacNSWindow class],
                 [InactiveSpaceDonorForTesting class],
                 @selector(isOnActiveSpace)) {}

  base::apple::ScopedObjCClassSwizzler swizzler;
};

ScopedSimulateInactiveSpace::ScopedSimulateInactiveSpace(
    views::Widget* browser_widget) {
  CHECK(!g_active_space_swizzler);
  CHECK(browser_widget);
  g_inactive_space_root_window =
      browser_widget->GetNativeWindow().GetNativeNSWindow();
  storage_ = std::make_unique<ObjCStorage>();
  g_active_space_swizzler = &storage_->swizzler;
}

ScopedSimulateInactiveSpace::~ScopedSimulateInactiveSpace() {
  storage_.reset();
  g_active_space_swizzler = nullptr;
  g_inactive_space_root_window = nil;
  [NSWorkspace.sharedWorkspace.notificationCenter
      postNotificationName:NSWorkspaceActiveSpaceDidChangeNotification
                    object:NSWorkspace.sharedWorkspace];
}

bool IsOmniboxPopupAttachedForTesting(const views::Widget* popup_widget) {
  NSWindow* ns_window =
      popup_widget ? popup_widget->GetNativeWindow().GetNativeNSWindow() : nil;
  return ns_window && ns_window.parentWindow;
}
