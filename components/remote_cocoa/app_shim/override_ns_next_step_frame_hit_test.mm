// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/override_ns_next_step_frame_hit_test.h"

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include "base/check.h"
#include "components/remote_cocoa/app_shim/immersive_mode_controller_cocoa.h"

// This file implements a workaround for a behavioral change in macOS 26
// fullscreen titlebar mouse event handling.
//
// In fullscreen, the window's border view is an instance of the private
// AppKit class NSNextStepFrame. When a mouse event occurs in the titlebar
// area, AppKit calls -[NSNextStepFrame _hitTestForEvent:] (or its inherited
// implementation) to determine which NSView should receive the event.
//
// Prior to macOS 26, a right-mouse-down on the tab strip would return
// Chrome's content view from this method. In macOS 26, it instead returns an
// internal AppKit view (e.g. NSToolbarView). This prevents Chrome from
// receiving the event and opening the context menu.
//
// To fix this, this file overrides -[NSNextStepFrame _hitTestForEvent:] to
// restore the old behavior.
//
// As of macOS 26, NSNextStepFrame does not originally have _hitTestForEvent.
// This implementation adds the method to NSNextStepFrame using class_addMethod.

namespace remote_cocoa {

static char kNSNextStepFrameHitTestTargetViewKey;

// Stores the hit test target view as an associated object on the window.
void SetNSNextStepFrameHitTestTargetView(NSWindow* fullscreen_window,
                                         NSView* target_view) {
  CHECK(IsNSToolbarFullScreenWindow(fullscreen_window));
  if (target_view) {
    CHECK_EQ(fullscreen_window, target_view.window);
  }
  objc_setAssociatedObject(fullscreen_window,
                           &kNSNextStepFrameHitTestTargetViewKey, target_view,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

NSView* GetNSNextStepFrameHitTestTargetView(NSWindow* window) {
  if (!IsNSToolbarFullScreenWindow(window)) {
    return nil;
  }
  return objc_getAssociatedObject(window,
                                  &kNSNextStepFrameHitTestTargetViewKey);
}

bool ShouldOverrideHitTesting(NSView* target_view, NSEvent* event) {
  if (!target_view) {
    return false;
  }
  CHECK(IsNSToolbarFullScreenWindow(target_view.window));
  NSPoint eventLocationInView = [target_view convertPoint:event.locationInWindow
                                                 fromView:nil];
  // Only override hit test result for right mouse down events inside of the
  // target view.
  return NSPointInRect(eventLocationInView, target_view.bounds) &&
         event.type == NSEventTypeRightMouseDown;
}

// Let -[NSNextStepFrame _hitTestForEvent:] return Chrome's NSView for right
// mouse down event in fullscreen.
void OverrideNSNextStepFrameHitTestInternal() {
  Class ns_next_step_frame_class = objc_getClass("NSNextStepFrame");
  CHECK(ns_next_step_frame_class);

  SEL selector = @selector(_hitTestForEvent:);

  // Get the method definition by searching the hierarchy. We need this for the
  // type encoding and, since the method is inherited, the inherited IMP.
  Method method = class_getInstanceMethod(ns_next_step_frame_class, selector);
  CHECK(method);

  const char* type_encoding = method_getTypeEncoding(method);
  // Check type encoding (assuming 64-bit architecture: id-24 @0 :8 @16).
  CHECK_EQ(std::string_view(type_encoding), "@24@0:8@16");

  using ImpFunctionType = NSView* (*)(id, SEL, NSEvent*);
  static ImpFunctionType g_old_imp;

  IMP new_imp =
      imp_implementationWithBlock(^NSView*(NSView* ns_frame, NSEvent* event) {
        NSWindow* window = ns_frame.window;
        if (!IsNSToolbarFullScreenWindow(window)) {
          return g_old_imp(ns_frame, selector, event);
        }

        NSView* target_view = GetNSNextStepFrameHitTestTargetView(window);
        if (ShouldOverrideHitTesting(target_view, event)) {
          return target_view;
        }

        return g_old_imp(ns_frame, selector, event);
      });

  BOOL added = class_addMethod(ns_next_step_frame_class, selector, new_imp,
                               type_encoding);
  // CHECK that NSNextStepFrame did not implement the method directly
  // This is true at least in macOS 26. The original behavior (g_old_imp) is the
  // inherited one.
  CHECK(added);
  g_old_imp =
      reinterpret_cast<ImpFunctionType>(method_getImplementation(method));

  CHECK(g_old_imp);
}

void OverrideNSNextStepFrameHitTest() {
  if (@available(macOS 26, *)) {
    static dispatch_once_t once_token;
    dispatch_once(&once_token, ^{
      OverrideNSNextStepFrameHitTestInternal();
    });
  }
}

}  // namespace remote_cocoa
