// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DOCK_ICON_H_
#define CHROME_BROWSER_UI_COCOA_DOCK_ICON_H_

#import <Cocoa/Cocoa.h>

#include "base/time/time.h"

// A class representing the dock icon of the Chromium app. It's its own class
// since several parts of the app want to manipulate the display of the dock
// icon.
//
// Like all UI, it must only be messaged from the UI thread.
@interface DockIcon : NSObject

+ (DockIcon*)sharedDockIcon;

// Updates the icon. Use the setters below to set the details first.
- (void)updateIcon;

// Download progress ///////////////////////////////////////////////////////////

// Indicates how many downloads are in progress.
- (void)setDownloads:(int)downloads;

// Indicates whether the progress indicator should be in an indeterminate state
// or not.
- (void)setIndeterminate:(BOOL)indeterminate;

// Indicates the amount of progress made of the download. Ranges from [0..1].
- (void)setProgress:(float)progress;

@end

#endif  // CHROME_BROWSER_UI_COCOA_DOCK_ICON_H_
