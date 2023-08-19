// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPS_TITLEBAR_BACKGROUND_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_APPS_TITLEBAR_BACKGROUND_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "third_party/skia/include/core/SkColor.h"

// A view that paints a solid color. Used to change the title bar background.
@interface TitlebarBackgroundView : NSView

// Adds a TitlebarBackgroundView to the [[window contentView] superView].
+ (TitlebarBackgroundView*)addToNSWindow:(NSWindow*)window
                             activeColor:(SkColor)activeColor
                           inactiveColor:(SkColor)inactiveColor;
@end

#endif  // CHROME_BROWSER_UI_COCOA_APPS_TITLEBAR_BACKGROUND_VIEW_H_
