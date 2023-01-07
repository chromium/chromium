// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/window_controls_overlay_nsview.h"

@implementation WindowControlsOverlayNSView

@synthesize bridge = _bridge;

- (instancetype)initWithBridge:
    (remote_cocoa::NativeWidgetNSWindowBridge*)bridge {
  if ((self = [super initWithFrame:NSZeroRect])) {
    _bridge = bridge;
  }
  return self;
}

- (NSView*)hitTest:(NSPoint)point {
  NSPoint pointInView = [self convertPoint:point fromView:self.superview];
  // This NSView is directly above NonClientView. We want to route events
  // to BridgedContentView so the right view in NonClientArea can consume them
  // instead of going to RenderWidgetHostView.
  if (NSPointInRect(pointInView, self.visibleRect))
    return self.superview;
  return [super hitTest:point];
}

- (void)updateBounds:(gfx::Rect)bounds {
  NSRect frameRect = bounds.ToCGRect();
  frameRect.origin.y = NSHeight(self.superview.bounds) - frameRect.origin.y -
                       NSHeight(frameRect);
  [self setFrame:frameRect];
}

@end
