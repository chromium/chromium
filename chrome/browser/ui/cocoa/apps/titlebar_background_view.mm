// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/apps/titlebar_background_view.h"

#include "base/check.h"
#import "skia/ext/skia_utils_mac.h"

@interface TitlebarBackgroundView ()
- (void)setColor:(NSColor*)color inactiveColor:(NSColor*)inactiveColor;
@end

@implementation TitlebarBackgroundView

+ (TitlebarBackgroundView*)addToNSWindow:(NSWindow*)window
                             activeColor:(SkColor)activeColor
                           inactiveColor:(SkColor)inactiveColor {
  DCHECK(window);
  // AppKit only officially supports adding subviews to the window's
  // contentView and not its superview (an NSNextStepFrame). The 10.10 SDK
  // allows adding an NSTitlebarAccessoryViewController to a window, but the
  // view can only be placed above the window control buttons, so we'd have to
  // replicate those.
  NSView* window_view = [[window contentView] superview];
  CGFloat height =
      NSHeight([window_view bounds]) - NSHeight([[window contentView] bounds]);
  base::scoped_nsobject<TitlebarBackgroundView> titlebar_background_view(
      [[TitlebarBackgroundView alloc]
          initWithFrame:NSMakeRect(0, NSMaxY([window_view bounds]) - height,
                                   NSWidth([window_view bounds]), height)]);
  [titlebar_background_view
      setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [window_view addSubview:titlebar_background_view
               positioned:NSWindowBelow
               relativeTo:nil];

  [titlebar_background_view setColor:skia::SkColorToSRGBNSColor(activeColor)
                       inactiveColor:skia::SkColorToSRGBNSColor(inactiveColor)];
  return titlebar_background_view.autorelease();
}

- (void)drawRect:(NSRect)rect {
  // Only the top corners are rounded. For simplicity, round all 4 corners but
  // draw the bottom corners outside of the visible bounds.
  CGFloat cornerRadius = 4.0;
  NSRect roundedRect = [self bounds];
  roundedRect.origin.y -= cornerRadius;
  roundedRect.size.height += cornerRadius;
  [[NSBezierPath bezierPathWithRoundedRect:roundedRect
                                   xRadius:cornerRadius
                                   yRadius:cornerRadius] addClip];
  if ([[self window] isMainWindow] || [[self window] isKeyWindow])
    [_color set];
  else
    [_inactiveColor set];
  NSRectFill(rect);
}

- (void)setColor:(NSColor*)color inactiveColor:(NSColor*)inactiveColor {
  _color.reset([color retain]);
  _inactiveColor.reset([inactiveColor retain]);
}

@end
