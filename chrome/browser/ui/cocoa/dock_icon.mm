// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/dock_icon.h"

#include <stdint.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/check_op.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

using content::BrowserThread;

namespace {

// The fraction of the size of the dock icon that the badge is.
constexpr CGFloat kBadgeFraction = 0.375f;
constexpr CGFloat kBadgeMargin = 4;
constexpr CGFloat kBadgeStrokeWidth = 6;

constexpr struct {
  CGFloat offset, radius, opacity;
} kBadgeShadows[] = {
    {0, 2, 0.14},
    {2, 2, 0.12},
    {1, 3, 0.2},
};

// The maximum update rate for the dock icon. 200ms = 5fps.
constexpr int64_t kUpdateFrequencyMs = 200;

}  // namespace

// A view that draws our dock tile.
@interface DockTileView : NSView

// Indicates how many downloads are in progress.
@property(nonatomic) int downloads;

// Indicates whether the progress indicator should be in an indeterminate state
// or not.
@property(nonatomic) BOOL indeterminate;

// Indicates the amount of progress made of the download. Ranges from [0..1].
@property(nonatomic) float progress;

@end

@implementation DockTileView

@synthesize downloads = _downloads;
@synthesize indeterminate = _indeterminate;
@synthesize progress = _progress;

- (void)drawRect:(NSRect)dirtyRect {
  // This needs to draw the current app icon, whether it's using the default
  // icon shipped or a custom icon.
  //
  // -[NSWorkspace iconForFile:] works, but it's NSString path-based, and APIs
  // that use those tend to be on the deprecation chopping block.
  //
  // The NSURLEffectiveIconKey resource value works, but it has an error path
  // that needs to be handled.
  //
  // -[NSApplication applicationIconImage] used to fail to return a custom icon
  // if set, which was fixed a while ago, but it returns an NSImage with a
  // single image rep, 32 pixels wide, which isn't good enough for detail work.
  //
  // Therefore, use [NSImage imageNamed:NSImageNameApplicationIcon].

  NSImage* appIcon = [NSImage imageNamed:NSImageNameApplicationIcon];
  [appIcon drawInRect:self.bounds
             fromRect:NSZeroRect
            operation:NSCompositingOperationSourceOver
             fraction:1.0];

  if (_downloads == 0) {
    return;
  }

  const CGFloat badgeSize = NSWidth(self.bounds) * kBadgeFraction;
  const NSRect badgeRect =
      NSMakeRect(NSMaxX(self.bounds) - badgeSize - kBadgeMargin, kBadgeMargin,
                 badgeSize, badgeSize);
  const CGFloat badgeRadius = badgeSize / 2;
  const NSPoint badgeCenter = NSMakePoint(NSMidX(badgeRect), NSMidY(badgeRect));

  NSBezierPath* backgroundPath =
      [NSBezierPath bezierPathWithOvalInRect:badgeRect];
  [NSColor.clearColor setFill];

  NSShadow* shadow = [[NSShadow alloc] init];
  shadow.shadowColor = NSColor.blackColor;
  for (const auto shadowProps : kBadgeShadows) {
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;
    shadow.shadowOffset = NSMakeSize(0, -shadowProps.offset);
    shadow.shadowBlurRadius = shadowProps.radius;
    [[NSColor colorWithCalibratedWhite:0 alpha:shadowProps.opacity] setFill];
    [shadow set];
    [backgroundPath fill];
  }

  [[NSColor colorWithCalibratedRed:0xec / 255.0
                             green:0xf3 / 255.0
                              blue:0xfe / 255.0
                             alpha:1] setFill];
  [backgroundPath fill];

  // Stroke
  if (!_indeterminate) {
    NSBezierPath* strokePath;
    if (_progress >= 1.0) {
      strokePath = [NSBezierPath bezierPathWithOvalInRect:badgeRect];
    } else {
      CGFloat endAngle = 90.0 - 360.0 * _progress;
      if (endAngle < 0.0) {
        endAngle += 360.0;
      }
      strokePath = [NSBezierPath bezierPath];
      [strokePath
          appendBezierPathWithArcWithCenter:badgeCenter
                                     radius:badgeRadius - kBadgeStrokeWidth / 2
                                 startAngle:90.0
                                   endAngle:endAngle
                                  clockwise:YES];
    }
    [strokePath setLineWidth:kBadgeStrokeWidth];
    // This color is GoogleBlue600, which matches the stroke color for the
    // progress ring of the download toolbar icon in a light theme.
    [[NSColor colorWithSRGBRed:0x1a / 255.0
                         green:0x73 / 255.0
                          blue:0xe8 / 255.0
                         alpha:1] setStroke];
    [strokePath stroke];
  }

  // Download count
  NSNumberFormatter* formatter = [[NSNumberFormatter alloc] init];
  NSString* countString = [formatter stringFromNumber:@(_downloads)];

  CGFloat countFontSize = 24;
  NSSize countSize = NSZeroSize;
  NSAttributedString* countAttrString = nil;
  while (true) {
    NSFont* countFont = [NSFont systemFontOfSize:countFontSize
                                          weight:NSFontWeightMedium];

    // This will generally be plain Helvetica.
    if (!countFont) {
      countFont = [NSFont userFontOfSize:countFontSize];
    }

    // Continued failure would generate an NSException.
    if (!countFont) {
      break;
    }

    countAttrString = [[NSAttributedString alloc]
        initWithString:countString
            attributes:@{
              NSForegroundColorAttributeName :
                  [NSColor colorWithCalibratedWhite:0 alpha:0.65],
              NSFontAttributeName : countFont,
            }];
    countSize = [countAttrString size];
    if (countSize.width > (badgeRadius - kBadgeStrokeWidth) * 1.5) {
      countFontSize -= 1.0;
    } else {
      break;
    }
  }

  NSPoint countOrigin = badgeCenter;
  countOrigin.x -= countSize.width / 2;
  countOrigin.y -= countSize.height / 2;

  [countAttrString drawAtPoint:countOrigin];
}

@end

@implementation DockIcon {
  // The time that the icon was last updated.
  base::TimeTicks _lastUpdate;

  // If true, the state has changed in a significant way since the last icon
  // update and throttling should not prevent icon redraw.
  BOOL _forceUpdate;
}

+ (DockIcon*)sharedDockIcon {
  static DockIcon* icon;
  if (!icon) {
    NSDockTile* dockTile = [NSApp dockTile];

    dockTile.contentView = [[DockTileView alloc] init];

    icon = [[DockIcon alloc] init];
  }

  return icon;
}

- (void)updateIcon {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::TimeDelta updateFrequency =
      base::Milliseconds(kUpdateFrequencyMs);

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta timeSinceLastUpdate = now - _lastUpdate;
  if (!_forceUpdate && timeSinceLastUpdate < updateFrequency) {
    return;
  }

  _lastUpdate = now;
  _forceUpdate = NO;

  NSDockTile* dockTile = NSApp.dockTile;

  [dockTile display];
}

- (void)setDownloads:(int)downloads {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DockTileView* dockTileView =
      base::apple::ObjCCast<DockTileView>(NSApp.dockTile.contentView);

  if (downloads != [dockTileView downloads]) {
    [dockTileView setDownloads:downloads];
    _forceUpdate = YES;
  }
}

- (void)setIndeterminate:(BOOL)indeterminate {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DockTileView* dockTileView =
      base::apple::ObjCCast<DockTileView>(NSApp.dockTile.contentView);

  if (indeterminate != [dockTileView indeterminate]) {
    [dockTileView setIndeterminate:indeterminate];
    _forceUpdate = YES;
  }
}

- (void)setProgress:(float)progress {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DockTileView* dockTileView =
      base::apple::ObjCCast<DockTileView>(NSApp.dockTile.contentView);

  [dockTileView setProgress:progress];
}

@end
