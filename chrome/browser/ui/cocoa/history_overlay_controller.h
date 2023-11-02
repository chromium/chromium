// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_HISTORY_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_HISTORY_OVERLAY_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"

@class HistoryOverlayView;

enum HistoryOverlayMode {
  kHistoryOverlayModeBack,
  kHistoryOverlayModeForward
};

// The HistoryOverlayController manages a view that is inserted atop the web
// contents to provide visual feedback when the user is performing history
// navigation gestures.
@interface HistoryOverlayController : NSViewController {
 @private
  HistoryOverlayMode _mode;
  // Strongly typed reference of self.view.
  base::scoped_nsobject<HistoryOverlayView> _contentView;
  // The view above which self.view is inserted as a subview.
  base::scoped_nsobject<NSView> _parent;
}

// Designated initializer.
- (instancetype)initForMode:(HistoryOverlayMode)mode;

// Shows the shield above |view|.
- (void)showPanelForView:(NSView*)view;

// Updates the appearance of the overlay based on track gesture progress.
// gestureAmount must be between 0 and 1.
// 0 indicates no progress. 1 indicates maximum progress.
// Finished indicates whether the gesture has reached maximum progress.
- (void)setProgress:(CGFloat)gestureAmount finished:(BOOL)finished;

// Fades the shield out and removes it from the view hierarchy.
- (void)dismiss;

@end

#endif  // CHROME_BROWSER_UI_COCOA_HISTORY_OVERLAY_CONTROLLER_H_
