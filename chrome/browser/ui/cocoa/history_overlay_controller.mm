// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/history_overlay_controller.h"

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

#import <QuartzCore/QuartzCore.h>

#include <cmath>

// Constants ///////////////////////////////////////////////////////////////////

// The radius of the circle drawn in the shield.
const CGFloat kShieldRadius = 70;

// The diameter of the circle and the width of its bounding box.
const CGFloat kShieldWidth = kShieldRadius * 2;

// The height of the shield.
const CGFloat kShieldHeight = 140;

// Additional height that is added to kShieldHeight when the gesture is
// considered complete.
const CGFloat kShieldHeightCompletionAdjust = 10;

// HistoryOverlayView //////////////////////////////////////////////////////////

// The content view that draws the semicircle and the arrow.
@interface HistoryOverlayView : NSView {
 @private
  HistoryOverlayMode mode_;
  CGFloat shieldAlpha_;
  base::scoped_nsobject<CAShapeLayer> shapeLayer_;
}
@property(nonatomic) CGFloat shieldAlpha;
- (id)initWithMode:(HistoryOverlayMode)mode
             image:(NSImage*)image;
@end

@implementation HistoryOverlayView

@synthesize shieldAlpha = shieldAlpha_;

- (id)initWithMode:(HistoryOverlayMode)mode
             image:(NSImage*)image {
  NSRect frame = NSMakeRect(0, 0, kShieldWidth, kShieldHeight);
  if ((self = [super initWithFrame:frame])) {
    mode_ = mode;
    shieldAlpha_ = 1.0;  // CAShapeLayer's fillColor defaults to opaque black.

    // A layer-hosting view.
    shapeLayer_.reset([[CAShapeLayer alloc] init]);
    [self setLayer:shapeLayer_];
    [self setWantsLayer:YES];

    // If going backward, the arrow needs to be in the right half of the circle,
    // so offset the X position.
    CGFloat offset = mode_ == kHistoryOverlayModeBack ? kShieldRadius : 0;
    NSRect arrowRect = NSMakeRect(offset, 0, kShieldRadius, kShieldHeight);
    arrowRect = NSInsetRect(arrowRect, 10, 0);  // Give a little padding.

    base::scoped_nsobject<NSImageView> imageView(
        [[NSImageView alloc] initWithFrame:arrowRect]);
    [imageView setImage:image];
    [imageView setAutoresizingMask:NSViewMinYMargin | NSViewMaxYMargin];
    [self addSubview:imageView];
  }
  return self;
}

- (void)setFrameSize:(CGSize)newSize {
  NSSize oldSize = [self frame].size;
  [super setFrameSize:newSize];

  if (![shapeLayer_ path] || !NSEqualSizes(oldSize, newSize))  {
    base::ScopedCFTypeRef<CGMutablePathRef> oval(CGPathCreateMutable());
    CGRect ovalRect = CGRectMake(0, 0, newSize.width, newSize.height);
    CGPathAddEllipseInRect(oval, nullptr, ovalRect);
    [shapeLayer_ setPath:oval];
  }
}

- (void)setShieldAlpha:(CGFloat)shieldAlpha {
  if (shieldAlpha != shieldAlpha_) {
    shieldAlpha_ = shieldAlpha;
    base::ScopedCFTypeRef<CGColorRef> fillColor(
        CGColorCreateGenericGray(0, shieldAlpha));
    [shapeLayer_ setFillColor:fillColor];
  }
}

@end

// HistoryOverlayController ////////////////////////////////////////////////////

@implementation HistoryOverlayController

- (id)initForMode:(HistoryOverlayMode)mode {
  if ((self = [super init])) {
    mode_ = mode;
    DCHECK(mode == kHistoryOverlayModeBack ||
           mode == kHistoryOverlayModeForward);
  }
  return self;
}

- (void)loadView {
  const gfx::Image& image =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          mode_ == kHistoryOverlayModeBack ? IDR_SWIPE_BACK
                                           : IDR_SWIPE_FORWARD);
  contentView_.reset(
      [[HistoryOverlayView alloc] initWithMode:mode_
                                         image:image.ToNSImage()]);
  self.view = contentView_;
}

- (void)setProgress:(CGFloat)gestureAmount finished:(BOOL)finished {
  NSRect parentFrame = [parent_ frame];
  // When tracking the gesture, the height is constant and the alpha value
  // changes from [0.25, 0.65].
  CGFloat height = kShieldHeight;
  CGFloat shieldAlpha = std::min(static_cast<CGFloat>(0.65),
                                 std::max(gestureAmount,
                                          static_cast<CGFloat>(0.25)));

  // When the gesture is very likely to be completed (90% in this case), grow
  // the semicircle's height and lock the alpha to 0.75.
  if (finished) {
    height += kShieldHeightCompletionAdjust;
    shieldAlpha = 0.75;
  }

  // Compute the new position based on the progress.
  NSRect frame = self.view.frame;
  frame.size.height = height;
  frame.origin.y = (NSHeight(parentFrame) / 2) - (height / 2);

  CGFloat width = std::min(kShieldRadius * gestureAmount, kShieldRadius);
  if (mode_ == kHistoryOverlayModeForward)
    frame.origin.x = NSMaxX(parentFrame) - width;
  else if (mode_ == kHistoryOverlayModeBack)
    frame.origin.x = NSMinX(parentFrame) - kShieldWidth + width;

  self.view.frame = frame;
  [contentView_ setShieldAlpha:shieldAlpha];
}

- (void)showPanelForView:(NSView*)view {
  parent_.reset([view retain]);
  [self setProgress:0 finished:NO];  // Set initial view position.
  [parent_ addSubview:self.view];
}

- (void)dismiss {
  const CGFloat kFadeOutDurationSeconds = 0.4;

  [NSAnimationContext beginGrouping];
  [NSAnimationContext currentContext].duration = kFadeOutDurationSeconds;
  [[self.view animator] removeFromSuperview];
  [NSAnimationContext endGrouping];
}

@end
