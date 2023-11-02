// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/history_overlay_controller.h"

#include "base/check.h"
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
  HistoryOverlayMode _mode;
  CGFloat _shieldAlpha;
  base::scoped_nsobject<CAShapeLayer> _shapeLayer;
}
@property(nonatomic) CGFloat shieldAlpha;
- (instancetype)initWithMode:(HistoryOverlayMode)mode image:(NSImage*)image;
@end

@implementation HistoryOverlayView

@synthesize shieldAlpha = _shieldAlpha;

- (instancetype)initWithMode:(HistoryOverlayMode)mode image:(NSImage*)image {
  NSRect frame = NSMakeRect(0, 0, kShieldWidth, kShieldHeight);
  if ((self = [super initWithFrame:frame])) {
    _mode = mode;
    _shieldAlpha = 1.0;  // CAShapeLayer's fillColor defaults to opaque black.

    // A layer-hosting view.
    _shapeLayer.reset([[CAShapeLayer alloc] init]);
    [self setLayer:_shapeLayer];
    [self setWantsLayer:YES];

    // If going backward, the arrow needs to be in the right half of the circle,
    // so offset the X position.
    CGFloat offset = _mode == kHistoryOverlayModeBack ? kShieldRadius : 0;
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

  if (![_shapeLayer path] || !NSEqualSizes(oldSize, newSize))  {
    base::ScopedCFTypeRef<CGMutablePathRef> oval(CGPathCreateMutable());
    CGRect ovalRect = CGRectMake(0, 0, newSize.width, newSize.height);
    CGPathAddEllipseInRect(oval, nullptr, ovalRect);
    [_shapeLayer setPath:oval];
  }
}

- (void)setShieldAlpha:(CGFloat)shieldAlpha {
  if (shieldAlpha != _shieldAlpha) {
    _shieldAlpha = shieldAlpha;
    base::ScopedCFTypeRef<CGColorRef> fillColor(
        CGColorCreateGenericGray(0, shieldAlpha));
    [_shapeLayer setFillColor:fillColor];
  }
}

@end

// HistoryOverlayController ////////////////////////////////////////////////////

@implementation HistoryOverlayController

- (instancetype)initForMode:(HistoryOverlayMode)mode {
  if ((self = [super init])) {
    _mode = mode;
    DCHECK(mode == kHistoryOverlayModeBack ||
           mode == kHistoryOverlayModeForward);
  }
  return self;
}

- (void)loadView {
  const gfx::Image& image =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          _mode == kHistoryOverlayModeBack ? IDR_SWIPE_BACK
                                           : IDR_SWIPE_FORWARD);
  _contentView.reset(
      [[HistoryOverlayView alloc] initWithMode:_mode
                                         image:image.ToNSImage()]);
  self.view = _contentView;
}

- (void)setProgress:(CGFloat)gestureAmount finished:(BOOL)finished {
  NSRect parentFrame = [_parent frame];
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
  if (_mode == kHistoryOverlayModeForward)
    frame.origin.x = NSMaxX(parentFrame) - width;
  else if (_mode == kHistoryOverlayModeBack)
    frame.origin.x = NSMinX(parentFrame) - kShieldWidth + width;

  self.view.frame = frame;
  [_contentView setShieldAlpha:shieldAlpha];
}

- (void)showPanelForView:(NSView*)view {
  _parent.reset([view retain]);
  [self setProgress:0 finished:NO];  // Set initial view position.
  [_parent addSubview:self.view];
}

- (void)dismiss {
  const CGFloat kFadeOutDurationSeconds = 0.4;

  [NSAnimationContext beginGrouping];
  [NSAnimationContext currentContext].duration = kFadeOutDurationSeconds;
  [[self.view animator] removeFromSuperview];
  [NSAnimationContext endGrouping];
}

@end
