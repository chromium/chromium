// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/omnibox/omnibox_popup_native_widget_mac.h"

#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"

namespace {

bool IsGeometryAttribute(NSAccessibilityAttributeName attribute) {
  return [attribute isEqualToString:NSAccessibilityPositionAttribute] ||
         [attribute isEqualToString:NSAccessibilitySizeAttribute];
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

// The root parent: in immersive fullscreen the direct parent is an overlay.
- (NSWindow*)windowManagementTarget {
  NSWindow* target = self;
  while (target.parentWindow) {
    target = target.parentWindow;
  }
  return target;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

- (id)accessibilityAttributeValue:(NSAccessibilityAttributeName)attribute {
  NSWindow* target = self.windowManagementTarget;
  if (target != self && IsGeometryAttribute(attribute)) {
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
