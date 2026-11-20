// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/omnibox/omnibox_popup_native_widget_mac.h"

#import <objc/runtime.h>

#include <algorithm>

#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"

namespace {

bool IsGeometryAttribute(NSAccessibilityAttributeName attribute) {
  return [attribute isEqualToString:NSAccessibilityPositionAttribute] ||
         [attribute isEqualToString:NSAccessibilitySizeAttribute];
}

// Reads proxied to the root parent. Moom only resizes a window that has a zoom
// button; the borderless popup has none, so it would move the browser but not
// resize it.
bool IsForwardedAttribute(NSAccessibilityAttributeName attribute) {
  return IsGeometryAttribute(attribute) ||
         [attribute isEqualToString:NSAccessibilityZoomButtonAttribute];
}

// Window menu actions that the borderless popup can't perform: Zoom, Minimize,
// and the private AppKit actions of Window > Fill, Center, Move & Resize, Full
// Screen Tile and, with Stage Manager, Remove Window from Set.
bool IsWindowMenuAction(SEL action) {
  static const SEL kActions[] = {
    @selector(performZoom:),
    @selector(performMiniaturize:),
    sel_registerName("_zoomFill:"),
    sel_registerName("_zoomCenter:"),
    sel_registerName("_zoomLeft:"),
    sel_registerName("_zoomRight:"),
    sel_registerName("_zoomTop:"),
    sel_registerName("_zoomBottom:"),
    sel_registerName("_zoomTopLeft:"),
    sel_registerName("_zoomTopRight:"),
    sel_registerName("_zoomBottomLeft:"),
    sel_registerName("_zoomBottomRight:"),
    sel_registerName("_zoomLeftAndRight:"),
    sel_registerName("_zoomRightAndLeft:"),
    sel_registerName("_zoomTopAndBottom:"),
    sel_registerName("_zoomBottomAndTop:"),
    sel_registerName("_zoomQuarters:"),
    sel_registerName("_zoomLeftThreeUp:"),
    sel_registerName("_zoomRightThreeUp:"),
    sel_registerName("_zoomTopThreeUp:"),
    sel_registerName("_zoomBottomThreeUp:"),
    sel_registerName("_zoomUntile:"),
    sel_registerName("_tileLeft:"),
    sel_registerName("_tileRight:"),
    sel_registerName("_removeWindowFromStageManagerSet:"),
  };
  return std::ranges::contains(kActions, action);
}

class OmniboxPopupNativeWidgetMac : public views::NativeWidgetMac {
 public:
  explicit OmniboxPopupNativeWidgetMac(views::Widget* widget)
      : views::NativeWidgetMac(widget) {}

 protected:
  // Mirrors NativeWidgetNSWindowBridge::CreateNSWindow() with our class. The
  // titlebar params are skipped since the popup is borderless.
  NativeWidgetMacNSWindow* CreateNSWindow(
      const remote_cocoa::mojom::CreateWindowParams* params) override {
    OmniboxPopupNSWindow* window = [[OmniboxPopupNSWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:params->style_mask
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window.releasedWhenClosed = NO;
    if (params->animation_enabled) {
      window.animationBehavior = NSWindowAnimationBehaviorDocumentWindow;
    }
    window.movable = NO;
    return window;
  }
};

}  // namespace

@implementation OmniboxPopupNSWindow

// AppKit keeps a window's top edge below the menu bar, except for a child
// window. The popup is shown before it's attached when its parent is off the
// active Space, e.g. in a new fullscreen window. It's always placed over its
// parent's location bar, so a constrained frame would leave it misaligned.
- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen*)screen {
  return frameRect;
}

// The popup stays open with an unfocused draft, which shouldn't keep the
// toolbar revealed in immersive fullscreen. While it isn't key, it moves
// offscreen along with its parent, the overlay window, when the toolbar hides.
- (BOOL)immersiveRevealLockRequiresKey {
  return YES;
}

// Focusing the omnibox makes the popup the key window, which would otherwise
// block AppKit cursor updates on its ancestor browser window.
- (BOOL)ancestorsUpdateCursorWhileKey {
  return YES;
}

// The root parent: in immersive fullscreen the direct parent is an overlay.
- (NSWindow*)windowManagementTarget {
  NSWindow* target = self;
  while (target.parentWindow) {
    target = target.parentWindow;
  }
  return target;
}

// The window whose commands this popup dispatches: the browser window, or the
// popup itself while it has no commandDispatchParent.
- (NSWindow*)commandDispatchRoot {
  NSWindow<CommandDispatchingWindow>* target = self;
  while (target.commandDispatchParent) {
    target = target.commandDispatchParent;
  }
  return target;
}

// AppKit sends the View > Full Screen menu action (e.g. Ctrl+Cmd+F) to the key
// window, and the borderless popup can't go fullscreen.
- (void)toggleFullScreen:(id)sender {
  if (NSWindow* target = self.commandDispatchRoot; target != self) {
    [target toggleFullScreen:sender];
  } else {
    [super toggleFullScreen:sender];
  }
}

// AppKit offers a Window menu action to each responder, starting at the key
// window, and sends it to the first one that responds or has a supplemental
// target. The borderless popup can't zoom, minimize or tile, so it declines
// these actions and names the browser window as their target.
- (BOOL)respondsToSelector:(SEL)aSelector {
  if (IsWindowMenuAction(aSelector) && self.commandDispatchRoot != self) {
    return NO;
  }
  return [super respondsToSelector:aSelector];
}

- (id)supplementalTargetForAction:(SEL)action sender:(id)sender {
  if (IsWindowMenuAction(action)) {
    NSWindow* target = self.commandDispatchRoot;
    if (target != self && [target respondsToSelector:action]) {
      return target;
    }
  }
  return [super supplementalTargetForAction:action sender:sender];
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

- (id)accessibilityAttributeValue:(NSAccessibilityAttributeName)attribute {
  NSWindow* target = self.windowManagementTarget;
  if (target != self && IsForwardedAttribute(attribute)) {
    return [target accessibilityAttributeValue:attribute];
  }
  return [super accessibilityAttributeValue:attribute];
}

// Must return the parent's answer: movable=NO makes AppKit say NO, and window
// managers then refuse the gesture instead of falling back to the parent.
- (BOOL)accessibilityIsAttributeSettable:
    (NSAccessibilityAttributeName)attribute {
  NSWindow* target = self.windowManagementTarget;
  if (target != self && IsGeometryAttribute(attribute)) {
    return [target accessibilityIsAttributeSettable:attribute];
  }
  return [super accessibilityIsAttributeSettable:attribute];
}

- (void)accessibilitySetValue:(id)value
                 forAttribute:(NSAccessibilityAttributeName)attribute {
  NSWindow* target = self.windowManagementTarget;
  if (target != self && IsGeometryAttribute(attribute)) {
    [target accessibilitySetValue:value forAttribute:attribute];
    return;
  }
  [super accessibilitySetValue:value forAttribute:attribute];
}

#pragma clang diagnostic pop

@end

views::NativeWidget* CreateOmniboxPopupNativeWidget(views::Widget* widget) {
  return new OmniboxPopupNativeWidgetMac(widget);
}
