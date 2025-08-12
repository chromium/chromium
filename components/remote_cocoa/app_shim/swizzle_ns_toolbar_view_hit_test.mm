// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/swizzle_ns_toolbar_view_hit_test.h"

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include "base/check.h"
#include "components/remote_cocoa/app_shim/immersive_mode_controller_cocoa.h"

@interface NSToolbarView : NSView
@end

// When a right mouse down event arrive at the titlebar area in fullscreen,
// AppKit will traverse the tree recursively and call -[NSView
// _hitTestForEvent:]. If any of the NSView returns non-nil, the view becomes
// the event target. Otherwise it falls back to find a target by calling the
// public -[NSView hitTest:]. This gives AppKit's interal classes a chance to
// intercept the event. Since macOS 26, the NSToolbarView starts to intercept
// right mouse events, effectively preventing Chrome from receiving it and
// subsequently opening the context menu. This file reverts the behavior by
// swizzling -[NSToolbarView _hitTestForEvent:] to return nil.
@interface NSToolbarView (Private)
- (NSView*)_hitTestForEvent:(NSEvent*)event;
@end

namespace remote_cocoa {

bool ShouldBypassNSToolbarViewForHitTesting(NSToolbarView* ns_toolbar_view,
                                            NSEvent* event) {
  // Only swizzle for right mouse down event in fullscreen.
  return IsNSToolbarFullScreenWindow(ns_toolbar_view.window) &&
         event.type == NSEventTypeRightMouseDown;
}

// Let -[NSToolbarView _hitTestForEvent:] return nil for right mouse down event
// in fullscreen. This is equivalent to no-op for pre-macOS 26 because
// _hitTestForEvent: is already returning nil there.
bool SwizzleNSToolbarViewHitTestInternal() {
  Class ns_toolbar_view_class = objc_getClass("NSToolbarView");
  if (!ns_toolbar_view_class) {
    return false;
  }

  SEL selector = @selector(_hitTestForEvent:);
  Method method = class_getInstanceMethod(ns_toolbar_view_class, selector);
  if (!method) {
    return false;
  }

  std::string_view type_encoding(method_getTypeEncoding(method));
  if (type_encoding != "@24@0:8@16") {
    return false;
  }

  using ImpFunctionType = NSView* (*)(id, SEL, NSEvent*);
  static ImpFunctionType g_old_imp;

  IMP new_imp =
      imp_implementationWithBlock(^NSView*(id object_self, NSEvent* event) {
        if (ShouldBypassNSToolbarViewForHitTesting(object_self, event)) {
          return nil;
        }
        return g_old_imp(object_self, selector, event);
      });

  g_old_imp = reinterpret_cast<ImpFunctionType>(
      method_setImplementation(method, new_imp));

  return !!g_old_imp;
}

void SwizzleNSToolbarViewHitTest() {
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    DCHECK(SwizzleNSToolbarViewHitTestInternal());
  });
}

}  // namespace remote_cocoa
